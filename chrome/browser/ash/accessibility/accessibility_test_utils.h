// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/input_method_observer.h"

namespace ash {

using ContextType = ::extensions::ExtensionBrowserTest::ContextType;
using ::extensions::ErrorConsole;

enum class ManifestVersion { kTwo, kThree };
class FullscreenMagnifierController;

// A class used to define the parameters of an API test case.
class ApiTestConfig {
 public:
  ApiTestConfig(ContextType context_type, ManifestVersion version)
      : context_type_(context_type), version_(version) {}

  ContextType context_type() const { return context_type_; }
  ManifestVersion version() const { return version_; }

 private:
  ContextType context_type_;
  ManifestVersion version_;
};

// A class that waits for caret bounds changed.
class CaretBoundsChangedWaiter : public ui::InputMethodObserver {
 public:
  explicit CaretBoundsChangedWaiter(ui::InputMethod* input_method);
  CaretBoundsChangedWaiter(const CaretBoundsChangedWaiter&) = delete;
  CaretBoundsChangedWaiter& operator=(const CaretBoundsChangedWaiter&) = delete;
  ~CaretBoundsChangedWaiter() override;

  // Waits for bounds changed within the input method.
  void Wait();

 private:
  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;

  raw_ptr<ui::InputMethod> input_method_;
  base::RunLoop run_loop_;
};

// Instantiate this class to get errors and warnings for an extension.
// This will catch console.error and console.warn messages as well as
// any uncaught JS errors in the extension and cause a non-fatal
// test failure as well as log the failure message.
//
// If this is used in the test SetUp, ensure the lifecycle lasts past
// the scope of the SetUp method, perhaps by using a member var, e.g.
// console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
//        browser()->profile(), extension_misc::kSelectToSpeakExtensionId);
class ExtensionConsoleErrorObserver : public ErrorConsole::Observer {
 public:
  ExtensionConsoleErrorObserver(Profile* profile, const char* extension_id);
  virtual ~ExtensionConsoleErrorObserver();

  // ErrorConsole::Observer:
  void OnErrorAdded(const extensions::ExtensionError* error) override;
  void OnErrorConsoleDestroyed() override;

  // Returns whether errors or warnings were received.
  bool HasErrorsOrWarnings();

  // A helper method to return the string content (in UTF8) of the error or
  // warning at the given |index|. This will cause a test failure if there is no
  // such message.
  std::string GetErrorOrWarningAt(size_t index) const;

  // Get the number of errors and warnings received.
  size_t GetErrorsAndWarningsCount() const;

 private:
  std::vector<std::u16string> errors_;
  raw_ptr<ErrorConsole> error_console_;
};

// Listens for changes to the histogram provided at construction. This class
// only allows `Wait()` to be called once. If you need to call `Wait()` multiple
// times, create multiple instances of this class.
class HistogramWaiter {
 public:
  explicit HistogramWaiter(const char* metric_name);
  ~HistogramWaiter();
  HistogramWaiter(const HistogramWaiter&) = delete;
  HistogramWaiter& operator=(const HistogramWaiter&) = delete;

  // Waits for the next update to the observed histogram.
  void Wait();
  void OnHistogramCallback(const char* metric_name,
                           uint64_t name_hash,
                           base::HistogramBase::Sample sample);

 private:
  std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      histogram_observer_;
  base::RunLoop run_loop_;
};

// FullscreenMagnifierController moves the magnifier window with animation
// when the magnifier is set to be enabled. This waiter class lets a consumer
// wait until the animation completes, i.e. after a mouse move.
class MagnifierAnimationWaiter {
 public:
  explicit MagnifierAnimationWaiter(FullscreenMagnifierController* controller);

  MagnifierAnimationWaiter(const MagnifierAnimationWaiter&) = delete;
  MagnifierAnimationWaiter& operator=(const MagnifierAnimationWaiter&) = delete;

  ~MagnifierAnimationWaiter();

  // Wait until the Fullscreen magnifier finishes animating.
  void Wait();

 private:
  void OnTimer();

  raw_ptr<FullscreenMagnifierController> controller_;  // not owned
  scoped_refptr<content::MessageLoopRunner> runner_;
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
