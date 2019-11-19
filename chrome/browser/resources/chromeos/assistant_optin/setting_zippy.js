// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'setting-zippy',

  properties: {
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
  },

  /**
   * Wrap the icon as a image into a html snippet.
   *
   * @param {string} iconUri the icon uri to be wrapped.
   * @param {string} imageLabel the aria label of the image.
   * @return {string} wrapped html snippet.
   *
   * @private
   */
  getWrappedIcon: function(iconUri, imageLabel) {
    return String.raw`
    <html>
      <style>
        body {
          margin: 0;
        }
        #icon {
          width: 20px;
          height: 20px;
          user-select: none;
        }
      </style>
    <body><img id="icon" aria-label="` +
        imageLabel + `" src="` + iconUri + '"></body></html>';
  },
});
