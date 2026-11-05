// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_WIN_SETTINGS_ROW_LOCATOR_WIN_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_WIN_SETTINGS_ROW_LOCATOR_WIN_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"
#include "chrome/browser/win/uia_element_locator_win.h"
#include "ui/gfx/geometry/rect.h"

// Finds the row that sets the default browser in the Windows Settings window
// and turns it into the arrow's offset from the window's top edge, in physical
// pixels. Records DefaultBrowser.VisualGuide.RowMeasurement*.
class SettingsRowLocatorWin {
 public:
  // Why a measurement produced no offset.
  using FailureReason = UiaElementLocatorWin::FailureReason;

  // The offset, in physical pixels, or why there is none.
  using Result = base::expected<int, FailureReason>;
  using MeasurementCallback = base::OnceCallback<void(Result)>;

  // The button's bounds relative to the window's origin, in physical pixels,
  // or why there are none.
  using ButtonResult = base::expected<gfx::Rect, FailureReason>;
  using ButtonCallback = base::OnceCallback<void(ButtonResult)>;

  // How a measurement ended, for the RowMeasurementOutcome histogram: the
  // failure reasons plus `kLocated`, so that one sample per measurement can be
  // recorded. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  // LINT.IfChange(UiaElementLocationOutcome)
  enum class Outcome {
    kLocated = 0,
    kNotFound = 1,
    kUnusable = 2,
    kTimedOut = 3,
    kDisconnected = 4,
    kSuperseded = 5,
    kWindowGone = 6,
    kMaxValue = kWindowGone,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:UiaElementLocationOutcome)

  SettingsRowLocatorWin();
  explicit SettingsRowLocatorWin(std::unique_ptr<UiaElementLocatorWin> locator);

  SettingsRowLocatorWin(const SettingsRowLocatorWin&) = delete;
  SettingsRowLocatorWin& operator=(const SettingsRowLocatorWin&) = delete;

  virtual ~SettingsRowLocatorWin();

  // Measures the row's offset in `settings_hwnd` and runs `callback` exactly
  // once, asynchronously, recording one sample. A measurement still in flight
  // is answered with `FailureReason::kSuperseded`. Destruction abandons a
  // measurement in flight: its callback is not run and no sample is recorded.
  // `settings_process` is a handle to the window's owner, opened with at least
  // PROCESS_QUERY_LIMITED_INFORMATION, which the service takes over. Callers
  // should open or duplicate it off the UI thread (see
  // base::Process::Duplicate()).
  virtual void Measure(HWND settings_hwnd,
                       base::Process settings_process,
                       MeasurementCallback callback);

  // Like Measure(), but answers with the "Set default" button's bounds and
  // records no histograms. Shares Measure()'s one request in flight.
  virtual void MeasureButton(HWND settings_hwnd,
                             base::Process settings_process,
                             ButtonCallback callback);

 private:
  // Asks the locator for the button, ending with `ending`, which OnLocated()
  // runs only while `this` is alive.
  void Locate(HWND settings_hwnd,
              base::Process settings_process,
              UiaElementLocatorWin::LocateCallback ending)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnLocated(UiaElementLocatorWin::LocateCallback ending,
                 UiaElementLocatorWin::Result result)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  std::unique_ptr<UiaElementLocatorWin> locator_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // Binds OnLocated(): the locator may answer a request from a posted task,
  // which must not reach `this` once it is gone.
  base::WeakPtrFactory<SettingsRowLocatorWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_WIN_SETTINGS_ROW_LOCATOR_WIN_H_
