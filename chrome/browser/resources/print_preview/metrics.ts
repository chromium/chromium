// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PrinterType} from './data/destination.js';
import {NativeLayer, NativeLayerImpl} from './native_layer.js';

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
 * Print Preview initialization events metrics buckets.
 */
export enum PrintPreviewInitializationEvents {
  // Function initiated.
  FUNCTION_INITIATED = 0,
  // Function completed succesfully.
  FUNCTION_SUCCESSFUL = 1,
  // Function failed.
  FUNCTION_FAILED = 2,
  // Max value.
  PRINT_PREVIEW_INITIALIZATION_EVENTS_MAX_BUCKET = 3
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
   * Destination Search specific usage statistics context.
   */
  static destinationSearch(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.DestinationAction',
        DestinationSearchBucket.DESTINATION_SEARCH_MAX_BUCKET);
  }

  /**
   * Print settings UI specific usage statistics context
   */
  static printSettingsUi(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.PrintSettingsUi',
        PrintSettingsUiBucket.PRINT_SETTINGS_UI_MAX_BUCKET);
  }

  /**
   * NativeLayer.getInitialSettings() specific usage statistics context
   */
  static getInitialSettings(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.Initialization.GetInitialSettings',
        PrintPreviewInitializationEvents
            .PRINT_PREVIEW_INITIALIZATION_EVENTS_MAX_BUCKET);
  }

  /**
   * NativeLayer.getPrinterCapabilities() specific usage statistics context
   */
  static getPrinterCapabilities(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.Initialization.GetPrinterCapabilities',
        PrintPreviewInitializationEvents
            .PRINT_PREVIEW_INITIALIZATION_EVENTS_MAX_BUCKET);
  }

  /**
   * NativeLayer.getPreview() specific usage statistics context
   */
  static getPreview(): MetricsContext {
    return new MetricsContext(
        'PrintPreview.Initialization.GetPreview',
        PrintPreviewInitializationEvents
            .PRINT_PREVIEW_INITIALIZATION_EVENTS_MAX_BUCKET);
  }

  /**
   * NativeLayer.getPrinters() specific usage statistics context
   */
  static getPrinters(type: PrinterType): MetricsContext {
    let histogram = '';
    switch (type) {
      case (PrinterType.EXTENSION_PRINTER):
        histogram = 'PrintPreview.Initialization.GetPrinters.Extension';
        break;
      case (PrinterType.PDF_PRINTER):
        histogram = 'PrintPreview.Initialization.GetPrinters.PDF';
        break;
      case (PrinterType.LOCAL_PRINTER):
        histogram = 'PrintPreview.Initialization.GetPrinters.Local';
        break;
      default:
        assertNotReached('unknown type = ' + type);
    }
    return new MetricsContext(
        histogram,
        PrintPreviewInitializationEvents
            .PRINT_PREVIEW_INITIALIZATION_EVENTS_MAX_BUCKET);
  }
}
