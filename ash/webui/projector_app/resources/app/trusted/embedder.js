// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {installLaunchHandler} from './launch.js';
import {ProjectorBrowserProxyImpl} from './projector_browser_proxy.js';
import {AppTrustedCommFactory, UntrustedAppClient} from './trusted_app_comm_factory.js';

/**
 * Gets the query string from the URL.
 * For example, if the URL is chrome://projector/annotator/abc?resourceKey=xyz,
 * then query is "abc?resourceKey=xyz".
 */
function getQuery() {
  if (!document.location.href) {
    return '';
  }
  const paths = document.location.href.split('/');
  if (paths.length < 1) {
    return '';
  }
  return paths[paths.length - 1];
}

Polymer({
  is: 'app-embedder',

  behaviors: [WebUIListenerBehavior],

  /** @override */
  ready() {
    document.body.querySelector('iframe').src =
        'chrome-untrusted://projector/' + getQuery();

    const client = AppTrustedCommFactory.getPostMessageAPIClient();

    this.addWebUIListener(
        'onNewScreencastPreconditionChanged', (precondition) => {
          client.onNewScreencastPreconditionChanged(precondition);
        });

    this.addWebUIListener('onSodaInstallProgressUpdated', (progress) => {
      if (isNaN(progress)) {
        console.error(
            'Invalid argument to onSodaInstallProgressUpdated', progress);
        return;
      }

      client.onSodaInstallProgressUpdated(progress);
    });

    this.addWebUIListener('onSodaInstalled', (args) => {
      client.onSodaInstalled();
    });

    this.addWebUIListener('onSodaInstallError', (args) => {
      client.onSodaInstallError();
    });

    // TODO(b/204372280): Rename onScreencastsStateChange to
    // OnPendingScreencastsStateChanged.
    this.addWebUIListener('onScreencastsStateChange', (pendingScreencasts) => {
      if (!Array.isArray(pendingScreencasts)) {
        console.error(
            'Invalid argument to onScreencastsStateChange', pendingScreencasts);
        return;
      }
      client.onScreencastsStateChange(pendingScreencasts);
    });
  },
});

installLaunchHandler();
