// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {addNode, addNodeWithText, setNodeDisplay} from './util.js';
import {DivView} from './view.js';

/** @type {?ProxyView} */
let instance = null;

/**
 * Indent |lines| by |start|.
 *
 * For example, if |start| = ' -> ' and |lines| = ['line1', 'line2', 'line3']
 * the output will be:
 *
 *   " -> line1\n" +
 *   "    line2\n" +
 *   "    line3"
 */
function indentLines(start, lines) {
  return start +
      lines.join(
          '\n' +
          ' '.repeat(start.length));
}

/**
 * Formats a proxy configuration dictionary (as produced by
 * net::ProxyConfig::ToValue()) into a human-readable string.
 */
function proxySettingsToString(config) {
  if (!config) {
    return '';
  }

  function getProxyListString(proxies) {
    if (Array.isArray(proxies)) {
      const listString = proxies.join(', ');
      if (proxies.length > 1) {
        return '[' + listString + ']';
      }
      return listString;
    }
    return proxies;
  }

  // The proxy settings specify major fallback choices (override rules, dynamic
  // routing, auto-detect, custom pac url, or manual settings).
  // We enumerate these to a list so we can later number them.
  const modes = [];

  // Output override rules before other modes.
  if (config.override_rules) {
    modes.push([
      'Override rules:',
      ...JSON.stringify(config.override_rules, null, 2).split('\n'),
    ]);
  }

  // Output dynamic routing config. These rules are evaluated after the
  // override rules, but before the automatic and manual settings.
  if (config.dynamic_routing_config) {
    modes.push([
      'Dynamic routing:',
      ...JSON.stringify(config.dynamic_routing_config, null, 2).split('\n'),
    ]);
  }

  // Output any automatic settings.
  if (config.auto_detect) {
    modes.push(['Auto-detect']);
  }
  if (config.pac_url) {
    modes.push(['PAC script: ' + config.pac_url]);
  }

  // Output any manual settings.
  if (config.single_proxy || config.proxy_per_scheme) {
    const lines = [];

    if (config.single_proxy) {
      lines.push('Proxy server: ' + getProxyListString(config.single_proxy));
    } else if (config.proxy_per_scheme) {
      for (const urlScheme in config.proxy_per_scheme) {
        if (urlScheme !== 'fallback') {
          lines.push(
              'Proxy server for ' + urlScheme.toUpperCase() + ': ' +
              getProxyListString(config.proxy_per_scheme[urlScheme]));
        }
      }
      if (config.proxy_per_scheme.fallback) {
        lines.push(
            'Proxy server for everything else: ' +
            getProxyListString(config.proxy_per_scheme.fallback));
      }
    }

    // Output any proxy bypass rules.
    if (config.bypass_list) {
      if (config.reverse_bypass) {
        lines.push('Reversed bypass list: ');
      } else {
        lines.push('Bypass list: ');
      }

      for (let i = 0; i < config.bypass_list.length; ++i) {
        lines.push('  ' + config.bypass_list[i]);
      }
    }

    modes.push(lines);
  }

  const result = [];
  if (modes.length < 1) {
    // If we didn't find any proxy settings modes, we are using DIRECT.
    result.push('Use DIRECT connections.');
  } else if (modes.length === 1) {
    // If there was just one mode, don't bother numbering it.
    result.push(modes[0].join('\n'));
  } else {
    // Otherwise concatenate all of the modes into a numbered list
    // (which correspond with the fallback order).
    for (let i = 0; i < modes.length; ++i) {
      result.push(indentLines('(' + (i + 1) + ') ', modes[i]));
    }
  }

  if (config.source !== undefined && config.source !== 'UNKNOWN') {
    result.push('Source: ' + config.source);
  }

  return result.join('\n');
}

/**
 * This view displays information on the proxy setup:
 *
 *   - Shows the current proxy settings.
 *   - Has a button to reload these settings.
 *   - Shows the list of proxy chains that are cached as "bad".
 *   - Has a button to clear the cached bad proxies.
 */
export class ProxyView extends DivView {
  constructor() {
    // Call superclass's constructor.
    super(ProxyView.MAIN_BOX_ID);

    this.browserBridge_ = BrowserBridge.getInstance();
    this.pollTimerId_ = null;

    // Hook up the UI components.
    $(ProxyView.RELOAD_SETTINGS_BUTTON_ID).onclick = () => {
      this.browserBridge_.sendReloadProxySettings();
      this.updateProxySettings();
    };
    $(ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID).onclick = () => {
      this.browserBridge_.sendClearBadProxies();
      this.updateProxySettings();
    };
    $(ProxyView.COPY_EFFECTIVE_SETTINGS_BUTTON_ID).onclick = () => {
      navigator.clipboard.writeText(
          $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
    };
    $(ProxyView.COPY_ORIGINAL_SETTINGS_BUTTON_ID).onclick = () => {
      navigator.clipboard.writeText(
          $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).textContent);
    };
  }

  show(isVisible) {
    super.show(isVisible);
    if (isVisible) {
      this.updateProxySettings();
    } else {
      this.clearPollTimer_();
    }
  }

  clearPollTimer_() {
    if (this.pollTimerId_ !== null) {
      window.clearTimeout(this.pollTimerId_);
      this.pollTimerId_ = null;
    }
  }

  schedulePoll_(delayMs) {
    this.clearPollTimer_();
    if (!this.isVisible()) {
      return;
    }
    this.pollTimerId_ = window.setTimeout(() => {
      this.pollTimerId_ = null;
      if (this.isVisible()) {
        this.updateProxySettings();
      }
    }, delayMs);
  }

  updateProxySettings() {
    this.clearPollTimer_();
    return this.browserBridge_.getProxySettings().then(proxySettings => {
      const result = this.onProxySettingsChanged(proxySettings);
      const isInitializing = Boolean(
          proxySettings && proxySettings.original && !proxySettings.effective);
      this.schedulePoll_(
          isInitializing ? ProxyView.INITIALIZING_POLL_INTERVAL_MS :
                           ProxyView.POLL_INTERVAL_MS);
      return result;
    });
  }

  onProxySettingsChanged(proxySettings) {
    $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).textContent = '';
    $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent = '';
    this.onBadProxiesChanged(null, 0);

    if (!proxySettings) {
      return false;
    }

    this.onBadProxiesChanged(
        proxySettings.badProxies, proxySettings.timeTickOffset);

    if (proxySettings.description) {
      $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent =
          proxySettings.description;
      setNodeDisplay($(ProxyView.ORIGINAL_CONTENT_DIV_ID), false);
      return true;
    }

    // Both |original| and |effective| are dictionaries describing the
    // settings.
    const original = proxySettings.original;
    const effective = proxySettings.effective;

    const originalStr = proxySettingsToString(original);
    const effectiveStr = (original && !effective) ?
        'Initializing...' :
        proxySettingsToString(effective);

    setNodeDisplay(
        $(ProxyView.ORIGINAL_CONTENT_DIV_ID),
        originalStr !== '' && originalStr !== effectiveStr);

    $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).textContent = originalStr;
    $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent = effectiveStr;

    return true;
  }

  onBadProxiesChanged(badProxies, timeTickOffset) {
    $(ProxyView.BAD_PROXIES_TBODY_ID).replaceChildren();
    setNodeDisplay(
        $(ProxyView.BAD_PROXIES_DIV_ID),
        Boolean(badProxies && badProxies.length > 0));

    if (!badProxies) {
      return false;
    }

    const offsetMs = Number(timeTickOffset || 0);
    for (let i = 0; i < badProxies.length; ++i) {
      const entry = badProxies[i];
      const badUntilDate = new Date(Number(entry.bad_until) + offsetMs);

      const tr = addNode($(ProxyView.BAD_PROXIES_TBODY_ID), 'tr');
      addNodeWithText(tr, 'td', entry.proxy_chain_uri);
      addNodeWithText(tr, 'td', badUntilDate.toLocaleString());
    }
    return true;
  }

  static getInstance() {
    return instance || (instance = new ProxyView());
  }
}

ProxyView.TAB_ID = 'tab-handle-proxy';
ProxyView.TAB_NAME = 'Proxy';
ProxyView.TAB_HASH = '#proxy';

// IDs for special HTML elements in index.html
ProxyView.MAIN_BOX_ID = 'proxy-view-tab-content';
ProxyView.RELOAD_SETTINGS_BUTTON_ID = 'proxy-view-reload-settings';
ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID = 'proxy-view-clear-bad-proxies';
ProxyView.COPY_EFFECTIVE_SETTINGS_BUTTON_ID =
    'proxy-view-copy-effective-settings';
ProxyView.COPY_ORIGINAL_SETTINGS_BUTTON_ID =
    'proxy-view-copy-original-settings';
ProxyView.ORIGINAL_SETTINGS_DIV_ID = 'proxy-view-original-settings';
ProxyView.EFFECTIVE_SETTINGS_DIV_ID = 'proxy-view-effective-settings';
ProxyView.ORIGINAL_CONTENT_DIV_ID = 'proxy-view-original-content';
ProxyView.EFFECTIVE_CONTENT_DIV_ID = 'proxy-view-effective-content';
ProxyView.BAD_PROXIES_DIV_ID = 'proxy-view-bad-proxies-div';
ProxyView.BAD_PROXIES_TBODY_ID = 'proxy-view-bad-proxies-tbody';
ProxyView.POLL_INTERVAL_MS = 1000;
ProxyView.INITIALIZING_POLL_INTERVAL_MS = 100;
