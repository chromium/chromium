// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './web_app_internals.mojom-lite.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'web-app-internals',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Whether Bookmark apps off Extensions is enabled.
     * @private {!boolean}
     */
    isBmoEnabled_: Boolean,

    /**
     * List of internal details about installed web apps.
     * @private {!Array<!mojom.webAppInternals.WebApp>}
     */
    webAppList_: Array,

    /**
     * Debug information about preinstalled web apps.
     * @private {!mojom.webAppInternals.PreinstalledWebAppDebugInfo|null}
     */
    preinstalledWebAppDebugInfo_: Object,

    /**
     * Prefs associated with non-user installed web apps.
     * @private {!string}
     */
    externallyInstalledWebAppPrefs_: String,
  },

  /**
   * @override
   * Fetches internal data about installed web apps from the browser.
   */
  ready() {
    (async () => {
      const remote =
          mojom.webAppInternals.WebAppInternalsPageHandler.getRemote();

      this.isBmoEnabled_ = (await remote.isBmoEnabled()).isBmoEnabled;

      this.webAppList_ = (await remote.getWebApps()).webAppList;
      this.webAppList_.sort((a, b) => a.name.localeCompare(b.name));

      this.preinstalledWebAppDebugInfo_ =
          (await remote.getPreinstalledWebAppDebugInfo()).status;

      this.externallyInstalledWebAppPrefs_ =
          (await remote.getExternallyInstalledWebAppPrefs())
              .externallyInstalledWebAppPrefs;

      this.highlightHashedId_();
      this.hashChangeListener_ = () => this.highlightHashedId_();
      window.addEventListener('hashchange', this.hashChangeListener_);

      this.$.saveButton.addEventListener('click', () => this.save());
      this.$.loadButton.addEventListener(
          'change', event => this.load(event.target.files[0]));
    })();
  },

  /**
   * @override
   * Cleans up global event listeners.
   */
  detached() {
    window.removeEventListener('hashchange', this.hashChangeListener_);
  },

  /**
   * Reads the current URL hash, scrolls the targeted element into view and
   * highlights it.
   * @private
   */
  highlightHashedId_() {
    for (const element of this.shadowRoot.querySelectorAll('.highlight')) {
      element.classList.remove('highlight');
    }

    if (!location.hash) {
      return;
    }

    const highlighted = this.shadowRoot.querySelector(location.hash);
    if (!highlighted) {
      return;
    }

    highlighted.scrollIntoView();
    highlighted.classList.add('highlight');
  },

  /**
   * Saves the data contents of the page to a JSON file.
   * @private
   */
  save() {
    const data = Object.fromEntries(
        Object.keys(this.properties).map(key => [key, this[key]]));
    const file = new Blob(
        [JSON.stringify(data, null, '  ')], {type: 'application/json'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(file);
    a.download = 'web-app-internals.json';
    a.click();
    URL.revokeObjectURL(a.href);
  },

  /**
   * Loads the given JSON file as the data contents of the page.
   * @param {File} file
   * @private
   * @suppress {checkTypes} Closure doesn't know about
         FileReader.readAsText(File).
   */
  load(file) {
    const reader = new FileReader();
    reader.addEventListener('load', event => {
      const json = JSON.parse(event.target.result);
      for (const key of Object.keys(this.properties)) {
        this[key] = json[key];
      }
    });
    reader.readAsText(file);
  },
});
