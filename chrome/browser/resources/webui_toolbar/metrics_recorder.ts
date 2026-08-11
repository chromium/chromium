// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Input type to activate the ReloadButton.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * Defined in //tools/metrics/histograms/metadata/ui/enums.xml.
 */
// LINT.IfChange(ReloadButtonInputType)
export enum ReloadButtonInputType {
  MOUSE_RELEASE = 0,
  KEY_PRESS = 1,
  COUNT = 2,
}
// LINT.ThenChange(
//   //tools/metrics/histograms/metadata/ui/enums.xml:ReloadButtonInputType,
//   //chrome/browser/ui/waap/waap_ui_metrics_recorder.h:ReloadButtonInputType
// )

/**
 * The visible mode of the ReloadButton.
 */
export enum ReloadButtonVisibleMode {
  // The "refresh" icon.
  RELOAD,
  // The "clear" icon.
  STOP,
}

/**
 * Class responsible for recording metrics related to the ReloadButton WebUI.
 */
export class MetricsRecorder {
  /**
   * Returns the visible mode based on the given loading state.
   * @param isLoading Whether the page is currently loading.
   * @return The target visible mode.
   */
  static getVisibleMode(isLoading: boolean): ReloadButtonVisibleMode {
    return isLoading ? ReloadButtonVisibleMode.STOP :
                       ReloadButtonVisibleMode.RELOAD;
  }
}
