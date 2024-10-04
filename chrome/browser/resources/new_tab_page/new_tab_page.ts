// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file exists to make tests work. Optimizing the NTP
 * flattens most JS files into a single new_tab_page.rollup.js. Therefore, tests
 * cannot import things from individual modules anymore. This file exports the
 * things tests need.
 */

export {SearchboxElement} from 'chrome://resources/cr_components/searchbox/searchbox.js';
export {SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
export {SearchboxIconElement} from 'chrome://resources/cr_components/searchbox/searchbox_icon.js';
export {SearchboxMatchElement} from 'chrome://resources/cr_components/searchbox/searchbox_match.js';
export {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
export {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
export {BrowserProxyImpl} from 'chrome://resources/js/metrics_reporter/browser_proxy.js';
export {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
export {getTrustedHTML} from 'chrome://resources/js/static_types.js';
export {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
export {AppElement, CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID, NtpCustomizeChromeEntryPoint, NtpElement} from './app.js';
export {BackgroundManager} from './background_manager.js';
export {CustomizeDialogPage} from './customize_dialog_types.js';
export {DoodleShareDialogElement} from './doodle_share_dialog.js';
export {IframeElement} from './iframe.js';
export {ProcessedFile, processFile} from './image_processor.js';
export {LogoElement} from './logo.js';
export {recordDuration, recordLoadDuration, recordOccurence, recordPerdecage} from './metrics_utils.js';
export {MobilePromoElement} from './mobile_promo.js';
export {NewTabPageProxy} from './new_tab_page_proxy.js';
export {checkTransparency, isBMP, isPNG, isWebP} from './transparency.js';
export {$$} from './utils.js';
export {Action as VoiceAction, Error as VoiceError} from './voice_search_overlay.js';
export {WindowProxy} from './window_proxy.js';
