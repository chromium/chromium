// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/win/settings_row_locator_win.h"

// <objbase.h> defines the COM `interface` macro that <uiautomation.h> (below)
// relies on, which WIN32_LEAN_AND_MEAN keeps <windows.h> from providing.
#include <objbase.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "ui/gfx/geometry/rect.h"

#include <uiautomation.h>

namespace {

using Outcome = SettingsRowLocatorWin::Outcome;

constexpr wchar_t kSettingsWindowClass[] = L"ApplicationFrameWindow";

// The "Set default" button on Settings > Apps > Default apps > <browser>.
// Older Windows 11 builds use "..._DefaultBrowserAction_Button"; the fragment
// matches both.
constexpr wchar_t kSetDefaultButtonId[] =
    L"SystemSettings_DefaultApps_DefaultBrowserWithPinToStartAndTaskbarAction_"
    L"Button";
constexpr wchar_t kSetDefaultIdFragment[] = L"defaultbrowser";

// Settings may open on the "Default apps" page before navigating to the
// browser's page.
constexpr base::TimeDelta kWaitForRow = base::Seconds(10);

constexpr char kOutcomeHistogram[] =
    "DefaultBrowser.VisualGuide.RowMeasurementOutcome";
constexpr char kDurationHistogram[] =
    "DefaultBrowser.VisualGuide.RowMeasurementTime";

// Returns the distance from the window's top edge down to the row's center,
// or nullopt if the center is not inside the window.
std::optional<int> OffsetFromMeasurement(
    const UiaElementLocatorWin::Measurement& measurement) {
  const int offset = measurement.element_bounds.CenterPoint().y() -
                     measurement.window_bounds.y();
  if (offset <= 0 || offset >= measurement.window_bounds.height()) {
    return std::nullopt;
  }
  return offset;
}

Outcome ToOutcome(UiaElementLocatorWin::FailureReason reason) {
  using FailureReason = UiaElementLocatorWin::FailureReason;
  switch (reason) {
    case FailureReason::kNotFound:
      return Outcome::kNotFound;
    case FailureReason::kUnusable:
      return Outcome::kUnusable;
    case FailureReason::kTimedOut:
      return Outcome::kTimedOut;
    case FailureReason::kDisconnected:
      return Outcome::kDisconnected;
    case FailureReason::kSuperseded:
      return Outcome::kSuperseded;
    case FailureReason::kWindowGone:
      return Outcome::kWindowGone;
  }
  NOTREACHED();
}

// Returns the element's bounds relative to the window's origin, or kUnusable
// if they are empty or their center is outside the window. The locator checks
// the same (see UiaElementLocatorWin), so a usable rect is never empty.
SettingsRowLocatorWin::ButtonResult BoundsInWindow(
    const UiaElementLocatorWin::Measurement& measurement) {
  if (measurement.element_bounds.IsEmpty() ||
      !measurement.window_bounds.Contains(
          measurement.element_bounds.CenterPoint())) {
    return base::unexpected(SettingsRowLocatorWin::FailureReason::kUnusable);
  }
  gfx::Rect bounds = measurement.element_bounds;
  bounds.Offset(-measurement.window_bounds.OffsetFromOrigin());
  return bounds;
}

// Measure()'s ending: records the samples and answers `callback`.
void ReplyWithRowOffset(SettingsRowLocatorWin::MeasurementCallback callback,
                        base::TimeTicks started,
                        UiaElementLocatorWin::Result result) {
  using FailureReason = SettingsRowLocatorWin::FailureReason;
  SettingsRowLocatorWin::Result reply =
      base::unexpected(FailureReason::kUnusable);
  Outcome outcome = Outcome::kUnusable;
  if (!result.has_value()) {
    reply = base::unexpected(result.error());
    outcome = ToOutcome(result.error());
  } else if (std::optional<int> offset = OffsetFromMeasurement(*result)) {
    reply = *offset;
    outcome = Outcome::kLocated;
  }

  base::UmaHistogramEnumeration(kOutcomeHistogram, outcome);
  // Not timed: a request a newer one cut short, or one refused because the
  // window was gone.
  if (outcome != Outcome::kSuperseded && outcome != Outcome::kWindowGone) {
    base::UmaHistogramMediumTimes(kDurationHistogram,
                                  base::TimeTicks::Now() - started);
  }

  std::move(callback).Run(std::move(reply));
}

// MeasureButton()'s ending.
void ReplyWithButtonBounds(SettingsRowLocatorWin::ButtonCallback callback,
                           UiaElementLocatorWin::Result result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }
  std::move(callback).Run(BoundsInWindow(*result));
}

}  // namespace

SettingsRowLocatorWin::SettingsRowLocatorWin()
    : SettingsRowLocatorWin(std::make_unique<UiaElementLocatorWin>()) {}

SettingsRowLocatorWin::SettingsRowLocatorWin(
    std::unique_ptr<UiaElementLocatorWin> locator)
    : locator_(std::move(locator)) {
  CHECK(locator_);
}

SettingsRowLocatorWin::~SettingsRowLocatorWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SettingsRowLocatorWin::Measure(HWND settings_hwnd,
                                    base::Process settings_process,
                                    MeasurementCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  Locate(settings_hwnd, std::move(settings_process),
         base::BindOnce(&ReplyWithRowOffset, std::move(callback),
                        base::TimeTicks::Now()));
}

void SettingsRowLocatorWin::MeasureButton(HWND settings_hwnd,
                                          base::Process settings_process,
                                          ButtonCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  Locate(settings_hwnd, std::move(settings_process),
         base::BindOnce(&ReplyWithButtonBounds, std::move(callback)));
}

void SettingsRowLocatorWin::Locate(
    HWND settings_hwnd,
    base::Process settings_process,
    UiaElementLocatorWin::LocateCallback ending) {
  // The locator answers a request this one supersedes with kSuperseded.
  locator_->Locate(
      {settings_hwnd, std::move(settings_process), kSettingsWindowClass},
      {kSetDefaultButtonId, kSetDefaultIdFragment, UIA_ButtonControlTypeId,
       kWaitForRow},
      base::BindOnce(&SettingsRowLocatorWin::OnLocated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(ending)));
}

void SettingsRowLocatorWin::OnLocated(
    UiaElementLocatorWin::LocateCallback ending,
    UiaElementLocatorWin::Result result) {
  std::move(ending).Run(std::move(result));
}
