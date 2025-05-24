// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NativeLayer} from './native_layer.js';
import {NativeLayerImpl} from './native_layer.js';

/**
 * Enumeration of buckets that a user can enter while using the destination
 * search widget.
 */
export enum DestinationSearchBucket {
  // Used when the print destination search widget is shown.
  DESTINATION_SHOWN = 0,
  // Used when the user selects a print destination.
  DESTINATION_CLOSED_CHANGED = 1,
  // Used when the print destination search widget is closed without selecting
  // a print destination.
  DESTINATION_CLOSED_UNCHANGED = 2,
  // Note: values 3-13 are intentionally unset as these correspond to
  // deprecated values in histograms/enums.xml. These enums are append-only.
  // User clicked on Manage button
  MANAGE_BUTTON_CLICKED = 14,
  // Max value.
  DESTINATION_SEARCH_MAX_BUCKET = 15
}

/**
 * Print settings UI usage metrics buckets.
 */
export enum PrintSettingsUiBucket {
  // Advanced settings dialog is shown.
  ADVANCED_SETTINGS_DIALOG_SHOWN = 0,
  // Advanced settings dialog is closed without saving a selection.
  ADVANCED_SETTINGS_DIALOG_CANCELED = 1,
  // 'More/less settings' expanded.
  MORE_SETTINGS_CLICKED = 2,
  // 'More/less settings' collapsed.
  LESS_SETTINGS_CLICKED = 3,
  // User printed with extra settings expanded.
  PRINT_WITH_SETTINGS_EXPANDED = 4,
  // User printed with extra settings collapsed.
  PRINT_WITH_SETTINGS_COLLAPSED = 5,
  // Max value.
  PRINT_SETTINGS_UI_MAX_BUCKET = 6
}

// <if expr="is_chromeos">
/**
 * Launch printer settings usage metric buckets.
 */
export enum PrintPreviewLaunchSourceBucket {
  // "Manage printers" button in preview-area when it fails to fetch
  // capabilities.
  PREVIEW_AREA_CONNECTION_ERROR = 0,
  // "Manage printers" button in destination-dialog-cros when there are no
  // destinations.
  DESTINATION_DIALOG_CROS_NO_PRINTERS = 1,
  // "Manage printers" button in destination-dialog-cros when there are
  // destinations.
  DESTINATION_DIALOG_CROS_HAS_PRINTERS = 2,
  // Max value.
  PRINT_PREVIEW_LAUNCH_SOURCE_MAX_BUCKET = 3,
}
// </if>

/* A context for recording a value in a specific UMA histogram. */
export class MetricsContext {
  private histogram_: string;
  private maxBucket_: number;
  private nativeLayer_: NativeLayer = NativeLayerImpl.getInstance();

  /**
   * @param histogram The name of the histogram to be recorded in.
   * @param maxBucket The max value for the last histogram bucket.
   */
  constructor(histogram: string, maxBucket: number) {
    this.histogram_ = histogram;
    this.maxBucket_ = maxBucket;
  }

  /**
   * Record a histogram value in UMA. If specified value is larger than the
   * max bucket value, record the value in the largest bucket
   * @param bucket Value to record.
   */
  record(bucket: number) {
    this.nativeLayer_.recordInHistogram(
        this.histogram_, (bucket > this.maxBucket_) ? this.maxBucket_ : bucket,
        this.maxBucket_);
  }

  /**
   * Print settings UI specific usage statistics context
   */
  static printSettingsUi(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.PrintSettingsUi',
        PrintSettingsUiBucket.PRINT_SETTINGS_UI_MAX_BUCKET);
  }

  // <if expr="is_chromeos">
  /**
   * Get `MetricsContext` for `PrintPreview.PrinterSettingsLaunchSource`
   * histogram.
   */
  static getLaunchPrinterSettingsMetricsContextCros(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.PrinterSettingsLaunchSource',
        PrintPreviewLaunchSourceBucket.PRINT_PREVIEW_LAUNCH_SOURCE_MAX_BUCKET);
  }
  // </if>
}
