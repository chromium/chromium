// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Minimal version of load_time_data.js for chrome-untrusted://
 * origins. They are sandboxed, so cannot use chrome://resources ("unable to
 * load local resource") which load_time_data.js relies on through strings.js.
 * Since we don't want to maintain a "mirror" of all the module dependencies on
 * each chrome-untrusted:// origin. For simplicity, this version lacks all the
 * validation done by load_time_data.js, and just aims to provide a compatible
 * API.
 */

/**
 * This file is a lightweight version copy of
 * ui/webui/resources/js/load_time_data.ts
 */

interface LoadTimeDataRaw {
  [key: string]: any;
}

class LoadTimeData {
  private data_: LoadTimeDataRaw|null = null;

  /**
   * Sets the backing object.
   *
   * Note that there is no getter for |data_| to discourage abuse of the form:
   *
   *     var value = loadTimeData.data()['key'];
   */
  set data(value: LoadTimeDataRaw) {
    this.data_ = value;
  }

  /**
   * @param id An ID of a value that might exist.
   * @return True if |id| is a key in the dictionary.
   */
  valueExists(id: string): boolean {
    if (this.data_) {
      return id in this.data_;
    }
    return false;
  }

  /**
   * Fetches a value, expecting that it exists.
   * @param id The key that identifies the desired value.
   * @return The corresponding value.
   */
  getValue(id: string): any {
    if (this.data_) {
      return this.data_[id];
    }
    return null;
  }

  /**
   * As above, but also makes sure that the value is a string.
   * @param id The key that identifies the desired string.
   * @return The corresponding string value.
   */
  getString(id: string): string {
    const value = this.getValue(id);
    return value;
  }

  /**
   * As above, but also makes sure that the value is a boolean.
   * @param id The key that identifies the desired boolean.
   * @return The corresponding boolean value.
   */
  getBoolean(id: string): boolean {
    const value = this.getValue(id);
    return value;
  }

  /**
   * As above, but also makes sure that the value is an integer.
   * @param id The key that identifies the desired number.
   * @return The corresponding number value.
   */
  getInteger(id: string): number {
    const value = this.getValue(id);
    return value;
  }
  /**
   * @return Whether loadTimeData.data has been set.
   */
  isInitialized(): boolean {
    return this.data_ !== null;
  }
}

const loadTimeData = new LoadTimeData();
// Expose |loadTimeData| directly on |window|, since within a JS module the
// scope is local and not all files have been updated to import the exported
// |loadTimeData| explicitly.
(window as any).loadTimeData = loadTimeData;
