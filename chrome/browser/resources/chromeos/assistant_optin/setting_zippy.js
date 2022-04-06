// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @polymer
 */
class SettingZippy extends Polymer.Element {
  static get is() {
    return 'setting-zippy';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      iconSrc: {
        type: String,
        value: null,
      },

      hideLine: {
        type: Boolean,
        value: false,
      },

      expandStyle: {
        type: Boolean,
        value: false,
      },

      toggleStyle: {
        type: Boolean,
        value: false,
      },

      popupStyle: {
        type: Boolean,
        value: false,
      },

      cardStyle: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * Wrap the icon as a image into a html snippet.
   *
   * @param {string} iconUri the icon uri to be wrapped.
   * @param {string} imageLabel the aria label of the image.
   * @param {string} background the background color of the icon, default to
   * white if unspecified.
   * @return {string} wrapped html snippet.
   *
   * @private
   */
  getWrappedIcon(iconUri, imageLabel, background) {
    return String.raw`
    <html>
      <style>
        body {
          margin: 0;
        }
        #icon {
          background: ` +
        (background || 'white') + `;
          width: 20px;
          height: 20px;
          user-select: none;
        }
      </style>
    <body><img id="icon" aria-label="` +
        imageLabel + `" src="` + iconUri + '"></body></html>';
  }
}

customElements.define(SettingZippy.is, SettingZippy);
