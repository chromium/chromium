// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::UTF8ToUTF16;
using std::u16string;

namespace extensions {

namespace {

const char kTestingPage[] = "/extensions/test_file.html";
const char kAnonymousFunction[] = "(anonymous function)";
const char* const kBackgroundPageName =
    extensions::kGeneratedBackgroundPageFilename;

const StackTrace& GetStackTraceFromError(const ExtensionError* error) {
  CHECK(error->type() == ExtensionError::Type::kRuntimeError);
  return (static_cast<const RuntimeError*>(error))->stack_trace();
}

// Verify that a given |frame| has the proper source and function name.
void CheckStackFrame(const StackFrame& frame,
                     const std::string& source,
                     const std::string& function) {
  EXPECT_EQ(base::UTF8ToUTF16(source), frame.source);
  EXPECT_EQ(base::UTF8ToUTF16(function), frame.function);
}

// Verify that all properties of a given |frame| are correct. Overloaded because
// we commonly do not check line/column numbers, as they are too likely
// to change.
void CheckStackFrame(const StackFrame& frame,
                     const std::string& source,
                     const std::string& function,
                     size_t line_number,
                     size_t column_number) {
  CheckStackFrame(frame, source, function);
  EXPECT_EQ(line_number, frame.line_number);
  EXPECT_EQ(column_number, frame.column_number);
}

// Verify that all properties of a given |error| are correct.
void CheckError(const ExtensionError* error,
                ExtensionError::Type type,
                const std::string& id,
                const std::string& source,
                bool from_incognito,
                const std::string& message) {
  ASSERT_TRUE(error);
  EXPECT_EQ(type, error->type());
  EXPECT_EQ(id, error->extension_id());
  EXPECT_EQ(base::UTF8ToUTF16(source), error->source());
  EXPECT_EQ(from_incognito, error->from_incognito());
  EXPECT_EQ(base::UTF8ToUTF16(message), error->message());
}

// Verify that all properties of a JS runtime error are correct.
void CheckRuntimeError(const ExtensionError* error,
                       const std::string& id,
                       const std::string& source,
                       bool from_incognito,
                       const std::string& message,
                       logging::LogSeverity level,
                       const GURL& context,
                       size_t expected_stack_size) {
  CheckError(error, ExtensionError::Type::kRuntimeError, id, source,
             from_incognito, message);

  const RuntimeError* runtime_error = static_cast<const RuntimeError*>(error);
  EXPECT_EQ(level, runtime_error->level());
  EXPECT_EQ(context, runtime_error->context_url());
  EXPECT_EQ(expected_stack_size, runtime_error->stack_trace().size());
}

void CheckManifestError(const ExtensionError* error,
                        const std::string& id,
                        const std::string& message,
                        const std::string& manifest_key,
                        const std::string& manifest_specific) {
  CheckError(error, ExtensionError::Type::kManifestError, id,
             // source is always the manifest for ManifestErrors.
             base::FilePath(kManifestFilename).AsUTF8Unsafe(),
             false,  // manifest errors are never from incognito.
             message);

  const ManifestError* manifest_error =
      static_cast<const ManifestError*>(error);
  EXPECT_EQ(manifest_key, manifest_error->manifest_key());
  EXPECT_EQ(base::UTF8ToUTF16(manifest_specific),
            manifest_error->manifest_specific());
}

// Checks that a given `error` refers to an error for using a deprecated
// manifest version.
void CheckDeprecatedManifestVersionError(const ExtensionError* error,
                                         const std::string& id) {
  CheckManifestError(error, id, manifest_errors::kManifestV2IsDeprecatedWarning,
                     manifest_keys::kManifestVersion,
                     std::string() /* no manifest_specific bit */);
}

}  // namespace

class ErrorConsoleBrowserTest : public ExtensionBrowserTest {
 public:
  ErrorConsoleBrowserTest() : error_console_(nullptr) {}
  ~ErrorConsoleBrowserTest() override {}

 protected:
  // A helper class in order to wait for the proper number of errors to be
  // caught by the ErrorConsole. This will run the MessageLoop until a given
  // number of errors are observed.
  // Usage:
  //   ...
  //   ErrorObserver observer(3, error_console);
  //   <Cause three errors...>
  //   observer.WaitForErrors();
  //   <Perform any additional checks...>
  class ErrorObserver : public ErrorConsole::Observer {
   public:
    ErrorObserver(size_t errors_expected, ErrorConsole* error_console)
        : errors_observed_(0),
          errors_expected_(errors_expected),
          waiting_(false),
          error_console_(error_console) {
      error_console_->AddObserver(this);
    }

    ErrorObserver(const ErrorObserver&) = delete;
    ErrorObserver& operator=(const ErrorObserver&) = delete;

    virtual ~ErrorObserver() {
      if (error_console_) {
        error_console_->RemoveObserver(this);
      }
    }

    // ErrorConsole::Observer implementation.
    void OnErrorAdded(const ExtensionError* error) override {
      ++errors_observed_;
      if (errors_observed_ >= errors_expected_) {
        if (waiting_)
          loop_.QuitWhenIdle();
      }
    }

    void OnErrorConsoleDestroyed() override { error_console_ = nullptr; }

    // Spin until the appropriate number of errors have been observed.
    void WaitForErrors() {
      if (errors_observed_ < errors_expected_) {
        waiting_ = true;
        loop_.Run();
        waiting_ = false;
      }
    }

   private:
    size_t errors_observed_;
    size_t errors_expected_;
    bool waiting_;

    base::RunLoop loop_;

    raw_ptr<ErrorConsole> error_console_;
  };

  // The type of action which we take after we load an extension in order to
  // cause any errors.
  enum Action {
    // Navigate to a (non-chrome) page to allow a content script to run.
    ACTION_NAVIGATE,
    // Simulate a browser action click.
    ACTION_BROWSER_ACTION,
    // Navigate to the new tab page.
    ACTION_NEW_TAB,
    // Do nothing (errors will be caused by a background script,
    // or by a manifest/loading warning).
    ACTION_NONE
  };

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Errors are only kept if we have Developer Mode enabled.
    profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

    error_console_ = ErrorConsole::Get(profile());
    CHECK(error_console_);

    test_data_dir_ = test_data_dir_.AppendASCII("error_console");
  }

  const GURL& GetTestURL() {
    if (test_url_.is_empty()) {
      CHECK(embedded_test_server()->Start());
      test_url_ = embedded_test_server()->GetURL(kTestingPage);
    }
    return test_url_;
  }

  // Load the extension at |path|, take the specified |action|, and wait for
  // |expected_errors| errors. Populate |extension| with a pointer to the loaded
  // extension.
  void LoadExtensionAndCheckErrors(const std::string& path,
                                   const LoadOptions& options,
                                   size_t errors_expected,
                                   Action action,
                                   const Extension** extension) {
    ErrorObserver observer(errors_expected, error_console_);
    *extension = LoadExtension(test_data_dir_.AppendASCII(path), options);
    ASSERT_TRUE(*extension);

    switch (action) {
      case ACTION_NAVIGATE: {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
        break;
      }
      case ACTION_BROWSER_ACTION: {
        ExtensionActionRunner::GetForWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->RunAction(*extension, true);
        break;
      }
      case ACTION_NEW_TAB: {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(
            browser(), GURL(chrome::kChromeUINewTabURL)));
        break;
      }
      case ACTION_NONE:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    observer.WaitForErrors();

    // We should only have errors for a single extension, or should have no
    // entries, if no errors were expected.
    ASSERT_EQ(errors_expected > 0 ? 1u : 0u,
              error_console()->get_num_entries_for_test());
    ASSERT_EQ(
        errors_expected,
        error_console()->GetErrorsForExtension((*extension)->id()).size());
  }

  ErrorConsole* error_console() { return error_console_; }

 private:
  // The URL used in testing for simple page navigations.
  GURL test_url_;

  // Weak reference to the ErrorConsole.
  raw_ptr<ErrorConsole, DanglingUntriaged> error_console_;
};

// Test to ensure that we are successfully reporting manifest errors as an
// extension is installed.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, ReportManifestErrors) {
  const Extension* extension = nullptr;
  // We expect two errors - one for an invalid permission, and a second for
  // an unknown key.
  LoadExtensionAndCheckErrors("manifest_warnings",
                              {.ignore_manifest_warnings = true}, 2,
                              ACTION_NONE, &extension);

  const ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  // Unfortunately, there's not always a hard guarantee of order in parsing the
  // manifest, so there's not a definitive order in which these errors may
  // occur. As such, we need to determine which error corresponds to which
  // expected error.
  const ExtensionError* permissions_error = nullptr;
  const ExtensionError* unknown_key_error = nullptr;
  const char kFakeKey[] = "not_a_real_key";
  for (const auto& error : errors) {
    ASSERT_EQ(ExtensionError::Type::kManifestError, error->type());
    const std::string& key =
        (static_cast<const ManifestError*>(error.get()))->manifest_key();
    if (key == manifest_keys::kPermissions) {
      permissions_error = error.get();
    } else if (key == kFakeKey) {
      unknown_key_error = error.get();
    }
  }
  ASSERT_TRUE(permissions_error);
  ASSERT_TRUE(unknown_key_error);

  const char kFakePermission[] = "not_a_real_permission";
  CheckManifestError(permissions_error,
                     extension->id(),
                     ErrorUtils::FormatErrorMessage(
                         manifest_errors::kPermissionUnknownOrMalformed,
                         kFakePermission),
                     manifest_keys::kPermissions,
                     kFakePermission);

  CheckManifestError(unknown_key_error,
                     extension->id(),
                     ErrorUtils::FormatErrorMessage(
                         manifest_errors::kUnrecognizedManifestKey,
                         kFakeKey),
                     kFakeKey,
                     std::string());
}

// Test that we do not store any errors unless the Developer Mode switch is
// toggled on the profile.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest,
                       DontStoreErrorsWithoutDeveloperMode) {
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);

  const Extension* extension = nullptr;
  // Same test as ReportManifestErrors, except we don't expect any errors since
  // we disable Developer Mode.
  LoadExtensionAndCheckErrors("manifest_warnings",
                              {.ignore_manifest_warnings = true}, 0,
                              ACTION_NONE, &extension);

  // Now if we enable developer mode, the errors should be reported...
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  EXPECT_EQ(2u, error_console()->GetErrorsForExtension(extension->id()).size());

  // ... and if we disable it again, all errors which we were holding should be
  // removed.
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
  EXPECT_EQ(0u, error_console()->GetErrorsForExtension(extension->id()).size());
}

// Load an extension which, upon visiting any page, first sends out a console
// log, and then crashes with a JS TypeError.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest,
                       ContentScriptLogAndRuntimeError) {
  const Extension* extension = nullptr;
  LoadExtensionAndCheckErrors(
      "content_script_log_and_runtime_error",
      {.ignore_manifest_warnings = false},
      2u,  // Two errors: A log message and a JS type error.
      ACTION_NAVIGATE, &extension);

  std::string script_url =
      extension->GetResourceURL("content_script.js").spec();

  const ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  // The extension logs a message with console.log(), then another with
  // console.warn(), and then triggers a TypeError.
  // There should be exactly two errors (the warning and the TypeError). The
  // error console ignores logs - this would tend to be too noisy, and doesn't
  // jive with the big `ERRORS` button in the UI.
  // See https://crbug.com/837401.
  ASSERT_EQ(2u, errors.size());

  // The first error should be a console log.
  CheckRuntimeError(errors[0].get(), extension->id(),
                    script_url,  // The source should be the content script url.
                    false,       // Not from incognito.
                    "warned message",  // The error message is the log.
                    logging::LOGGING_WARNING,
                    GetTestURL(),  // Content scripts run in the web page.
                    2u);

  const StackTrace& stack_trace1 = GetStackTraceFromError(errors[0].get());
  CheckStackFrame(stack_trace1[0], script_url,
                  "warnMessage",  // function name
                  10u,            // line number
                  11u /* column number */);

  CheckStackFrame(stack_trace1[1], script_url, kAnonymousFunction, 14u, 1u);

  // The second error should be a runtime error.
  CheckRuntimeError(
      errors[1].get(), extension->id(), script_url,
      false,  // not from incognito
      "Uncaught TypeError: "
      "Cannot set properties of undefined (setting 'foo')",
      logging::LOGGING_ERROR,  // JS errors are always ERROR level.
      GetTestURL(), 1u);

  const StackTrace& stack_trace2 = GetStackTraceFromError(errors[1].get());
  CheckStackFrame(stack_trace2[0], script_url, kAnonymousFunction, 17u, 1u);
}

// Catch an error from a BrowserAction; this is more complex than a content
// script error, since browser actions are routed through our own code.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, BrowserActionRuntimeError) {
  const Extension* extension = nullptr;
  LoadExtensionAndCheckErrors(
      "browser_action_runtime_error", {.ignore_manifest_warnings = false},
      // Two errors: An error for running a deprecated manifest version and
      // a reference error from within the browser action.
      2u, ACTION_BROWSER_ACTION, &extension);

  std::string script_url =
      extension->GetResourceURL("browser_action.js").spec();

  const ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  CheckDeprecatedManifestVersionError(errors[0].get(), extension->id());

  // TODO(devlin): The specific event name (here, 'browserAction.onClicked')
  // may or may not be worth preserving. In most cases, it's unnecessary with
  // the line number, but it could be useful in some cases.
  std::string message =
      "Error in event handler: ReferenceError: baz is not defined";

  CheckRuntimeError(errors[1].get(), extension->id(), script_url,
                    false,  // not incognito
                    message, logging::LOGGING_ERROR,
                    extension->GetResourceURL(kBackgroundPageName), 1u);

  const StackTrace& stack_trace = GetStackTraceFromError(errors[1].get());
  // Note: This test used to have a stack trace of length 6 that contains stack
  // frames in the extension code, but since crbug.com/404406 was fixed only
  // stack frames within user-defined extension code are printed.

  CheckStackFrame(stack_trace[0], script_url, kAnonymousFunction);
}

// Test that we can catch an error for calling an API with improper arguments.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, BadAPIArgumentsRuntimeError) {
  const Extension* extension = nullptr;
  LoadExtensionAndCheckErrors(
      "bad_api_arguments_runtime_error", {.ignore_manifest_warnings = false},
      // Two errors: An error for running a deprecated manifest version and an
      // error for calling an API with improper arguments.
      2, ACTION_NONE, &extension);

  const ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  CheckDeprecatedManifestVersionError(errors[0].get(), extension->id());

  std::string source = extension->GetResourceURL("background.js").spec();
  std::string message =
      "Uncaught TypeError: Error in invocation of tabs.get"
      "(integer tabId, function callback): No matching signature.";

  CheckRuntimeError(errors[1].get(), extension->id(), source,
                    false,  // not incognito
                    message, logging::LOGGING_ERROR,
                    extension->GetResourceURL(kBackgroundPageName), 1u);

  const StackTrace& stack_trace = GetStackTraceFromError(errors[1].get());
  ASSERT_EQ(1u, stack_trace.size());
  CheckStackFrame(stack_trace[0], source, kAnonymousFunction);
}

// Test that we catch an error when we try to call an API method without
// permission.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, BadAPIPermissionsRuntimeError) {
  const Extension* extension = nullptr;
  LoadExtensionAndCheckErrors(
      "bad_api_permissions_runtime_error", {.ignore_manifest_warnings = false},
      // Two errors: an error for running a deprecated manifest version and an
      // error for trying to call addUrl() on chrome.history without permission
      // which results in a TypeError.
      2, ACTION_NONE, &extension);

  std::string script_url = extension->GetResourceURL("background.js").spec();

  const ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());

  CheckDeprecatedManifestVersionError(errors[0].get(), extension->id());

  CheckRuntimeError(errors[1].get(), extension->id(), script_url,
                    false,  // not incognito
                    "Uncaught TypeError: Cannot read properties of undefined "
                    "(reading 'addUrl')",
                    logging::LOGGING_ERROR,
                    extension->GetResourceURL(kBackgroundPageName), 1u);

  const StackTrace& stack_trace = GetStackTraceFromError(errors[1].get());
  ASSERT_EQ(1u, stack_trace.size());
  CheckStackFrame(stack_trace[0],
                  script_url,
                  kAnonymousFunction,
                  5u, 1u);
}

// Test that if there is an error in an HTML page loaded by an extension (most
// common with apps), it is caught and reported by the ErrorConsole.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, BadExtensionPage) {
  const Extension* extension = nullptr;
  LoadExtensionAndCheckErrors(
      "bad_extension_page", {.ignore_manifest_warnings = false},
      1,  // One error: the page will load JS which has a reference error.
      ACTION_NEW_TAB, &extension);
}

// Test that extension errors that go to chrome.runtime.lastError are caught
// and reported by the ErrorConsole.
// TODO(crbug.com/40750922) Flaky on many builders.
IN_PROC_BROWSER_TEST_F(ErrorConsoleBrowserTest, DISABLED_CatchesLastError) {
  const Extension* extension = nullptr;
  LoadExtensionAndCheckErrors(
      "trigger_last_error", {.ignore_manifest_warnings = false},
      1,  // One error, which is sent through last error when trying to remove
          // a non-existent permisison.
      ACTION_NONE, &extension);

  const ErrorList& errors =
      error_console()->GetErrorsForExtension(extension->id());
  ASSERT_EQ(1u, errors.size());

  // TODO(devlin): This is unfortunate. We lose a lot of context by using
  // RenderFrame::AddMessageToConsole() instead of console.error(). This could
  // be expanded; blink::SourceLocation knows how to capture an inspector
  // stack trace.
  std::string source =
      extension->GetResourceURL(kGeneratedBackgroundPageFilename).spec();
  // Line number '0' comes from errors that are logged to the render frame
  // directly (e.g. background_age.html (0)).
  size_t line_number = 0;
  // Column number remains at the default specified in StackFrame (1).
  size_t column_number = 1;
  std::string message =
      "Unchecked runtime.lastError: 'foobar' is not a recognized permission.";

  CheckRuntimeError(errors[0].get(), extension->id(), source,
                    false,  // not incognito
                    message, logging::LOGGING_ERROR,
                    extension->GetResourceURL(kBackgroundPageName), 1u);

  const StackTrace& stack_trace = GetStackTraceFromError(errors[0].get());
  ASSERT_EQ(1u, stack_trace.size());
  CheckStackFrame(stack_trace[0], source, kAnonymousFunction, line_number,
                  column_number);
}

}  // namespace extensions
