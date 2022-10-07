// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class DocUtils {
  /**
   * Creates a data url for a document.
   * @param {string} doc Snippet
   * @return {string}
   */
  static createUrlForDoc(doc) {
    const docString = doc.toString();

    return 'data:text/html,<!doctype html>' +
        encodeURIComponent(DocUtils.collapseWhitespace(
            docString.replace(/[\n\r]/g, '').trim()));
  }

  /**
   * Collapses inner whitespace.
   * @param {string} str
   * @return {string}
   */
  static collapseWhitespace(str) {
    return str.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');
  }
}
