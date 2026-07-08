// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/test_util.h"

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_registry_test_helper.h"

namespace dictation {

TargetId EmptyTargetId() {
  return TargetId();
}

TargetId DefaultInPageTargetId(content::WebContents* web_contents) {
  return TargetId{web_contents->GetPrimaryMainFrame()->GetWeakDocumentPtr()};
}

base::test::ScopedFeatureList CreateEnablingFeatureList() {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kDictation, {{"use_component_extension", "false"}});
  return feature_list;
}

const extensions::Extension* LoadTestExtensionInManualMode(Profile* profile) {
  extensions::ExtensionRegistryTestHelper observer(
      std::string(kDictationTestExtensionId).c_str(), profile);
  base::FilePath test_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath extension_path =
      test_data_dir.AppendASCII("extensions").AppendASCII("dictation");
  extensions::ChromeTestExtensionLoader loader(profile);
  const extensions::Extension* ext = loader.LoadExtension(extension_path).get();
  EXPECT_TRUE(ext);
  EXPECT_EQ(ext->id(), kDictationTestExtensionId);
  observer.WaitForServiceWorkerStart();

  std::string script = R"JS(
    (async function() {
      await chrome.storage.local.set({manualTest: true});
      chrome.test.sendScriptResult('ready');
    })();
  )JS";

  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, std::string(kDictationTestExtensionId), script);
  EXPECT_EQ("ready", result);

  return ext;
}

void ExtensionSendTranscriptUpdate(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id,
    extensions::api::dictation_private::TranscriptionType type,
    std::string_view data) {
  std::string script = content::JsReplace(
      R"JS(
    (async function() {
      try {
        await chrome.dictationPrivate.updateTranscription({
          streamId: $1,
          type: $2,
          data: $3
        });
        chrome.test.sendScriptResult('success');
      } catch (e) {
        chrome.test.sendScriptResult('error: ' + e.message);
      }
    })();
  )JS",
      stream_id.value(), extensions::api::dictation_private::ToString(type),
      data);

  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, std::string(kDictationTestExtensionId), script);
  CHECK_EQ("success", result.GetString());
}

void ExtensionSendStreamStateUpdate(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id,
    extensions::api::dictation_private::StreamState state) {
  std::string script = content::JsReplace(
      R"JS(
    (async function() {
      try {
        await chrome.dictationPrivate.setStreamState({
            streamId: $1,
            state: $2
        });
        chrome.test.sendScriptResult('success');
      } catch (e) {
        chrome.test.sendScriptResult('error: ' + e.message);
      }
    })();
      )JS",
      stream_id.value(), extensions::api::dictation_private::ToString(state));

  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, std::string(kDictationTestExtensionId), script);
  CHECK_EQ("success", result.GetString());
}

void ExtensionWaitForStreamStart(Profile* profile,
                                 DictationMultiplexer::StreamId stream_id) {
  std::string script = content::JsReplace(
      R"JS(
    (async function() {
      try {
        await globalThis.waitForStreamStart($1);
        chrome.test.sendScriptResult('success');
      } catch (e) {
        chrome.test.sendScriptResult('error: ' + e.message);
      }
    })();
      )JS",
      stream_id.value());

  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, std::string(kDictationTestExtensionId), script);
  CHECK_EQ("success", result.GetString());
}

namespace {

std::optional<DictationContext> ParseDictationContext(
    const base::DictValue& dict) {
  if (!dict.FindBool("hasContext").value_or(false)) {
    return std::nullopt;
  }

  DictationContext context;

  const std::string* editable_content = dict.FindString("editableContent");
  if (editable_content) {
    context.editable_content = *editable_content;
  }

  const std::string* inner_text = dict.FindString("innerText");
  if (inner_text) {
    context.inner_text = *inner_text;
  }

  const base::ListValue* apc_bytes = dict.FindList("annotatedPageContent");
  if (apc_bytes) {
    optimization_guide::proto::AnnotatedPageContent apc_proto;
    std::vector<uint8_t> bytes;
    bytes.reserve(apc_bytes->size());
    for (const auto& val : *apc_bytes) {
      CHECK(val.is_int());
      bytes.push_back(static_cast<uint8_t>(val.GetInt()));
    }
    CHECK(apc_proto.ParseFromArray(bytes.data(), bytes.size()));
    context.annotated_page_content = std::move(apc_proto);
  }

  return context;
}

}  // namespace

std::optional<DictationContext> ExtensionGetStartStreamDetails(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id) {
  std::string script = content::JsReplace(
      R"JS(
    (async function() {
      try {
        const details = await globalThis.waitForStreamStart($1);
        const result = {
          success: true,
          hasContext: details.context !== undefined
        };
        const context = details.context;
        if (context) {
          if (context.annotatedPageContent) {
            result.annotatedPageContent =
                Array.from(new Uint8Array(context.annotatedPageContent));
          }
          if (context.innerText !== undefined) {
            result.innerText = context.innerText;
          }
          if (context.editableContent !== undefined) {
            result.editableContent = context.editableContent;
          }
        }
        chrome.test.sendScriptResult(result);
      } catch (e) {
        chrome.test.sendScriptResult({ success: false, error: e.message });
      }
    })();
      )JS",
      stream_id.value());

  base::Value result_value =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, std::string(kDictationTestExtensionId), script);

  CHECK(result_value.is_dict())
      << "Expected dictionary result, got: " << result_value;
  const base::DictValue& dict = result_value.GetDict();
  CHECK(dict.FindBool("success").value_or(false))
      << "Failed to get dictation context: " << *dict.FindString("error");

  return ParseDictationContext(dict);
}

DictationContext ExtensionGetUpdatedContext(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id) {
  std::string script = content::JsReplace(
      R"JS(
    (async function() {
      try {
        const details = await globalThis.waitForContextUpdate($1);
        const result = {
          success: true,
          hasContext: true
        };
        const context = details.context;
        if (context.annotatedPageContent) {
          result.annotatedPageContent =
              Array.from(new Uint8Array(context.annotatedPageContent));
        }
        if (context.innerText !== undefined) {
          result.innerText = context.innerText;
        }
        if (context.editableContent !== undefined) {
          result.editableContent = context.editableContent;
        }
        chrome.test.sendScriptResult(result);
      } catch (e) {
        chrome.test.sendScriptResult({ success: false, error: e.message });
      }
    })();
      )JS",
      stream_id.value());

  base::Value result_value =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, std::string(kDictationTestExtensionId), script);

  CHECK(result_value.is_dict())
      << "Expected dictionary result, got: " << result_value;
  const base::DictValue& dict = result_value.GetDict();
  CHECK(dict.FindBool("success").value_or(false))
      << "Failed to get dictation context: " << *dict.FindString("error");

  std::optional<DictationContext> context = ParseDictationContext(dict);
  CHECK(context.has_value());
  return std::move(*context);
}

using ::testing::_;

MockStreamProvider::MockStreamProvider() = default;
MockStreamProvider::~MockStreamProvider() = default;

MockSessionUi::MockSessionUi() = default;
MockSessionUi::~MockSessionUi() = default;

MockSessionControllerDelegate::MockSessionControllerDelegate() {
  ON_CALL(*this, CreateUi(_)).WillByDefault([]() {
    return std::make_unique<testing::NiceMock<MockSessionUi>>();
  });
  ON_CALL(*this, CreateStreamProvider(_)).WillByDefault([]() {
    return std::make_unique<testing::NiceMock<MockStreamProvider>>();
  });
}
MockSessionControllerDelegate::~MockSessionControllerDelegate() = default;

MockDictationKeyedService::MockDictationKeyedService(Profile* profile)
    : DictationKeyedService(profile) {}
MockDictationKeyedService::~MockDictationKeyedService() = default;

std::unique_ptr<StreamProvider> MockDictationKeyedService::CreateStreamProvider(
    SessionController& controller) const {
  return std::make_unique<testing::NiceMock<MockStreamProvider>>();
}
std::unique_ptr<SessionUi> MockDictationKeyedService::CreateUi(
    SessionController& controller) const {
  return std::make_unique<testing::NiceMock<MockSessionUi>>();
}

}  // namespace dictation
