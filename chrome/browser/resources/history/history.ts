// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export {browserProxyFactory as foreignSessionBrowserProxyFactory} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
export type {HistoryEntry, QueryResult} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
export {browserProxyFactory as historyClustersBrowserProxyFactory, ClusterAction, PageCallbackRouter, PageHandlerRemote, RelatedSearchAction, VisitAction, VisitType} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
export type {MetricsProxy} from 'chrome://resources/cr_components/history_clusters/metrics_proxy.js';
export {MetricsProxyImpl} from 'chrome://resources/cr_components/history_clusters/metrics_proxy.js';
export type {HistoryEmbeddingsMoreActionsClickEvent} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
export {browserProxyFactory as historyEmbeddingsBrowserProxyFactory} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
export type {BrowserProxy as HistoryEmbeddingsBrowserProxy} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
export {PageHandlerRemote as HistoryEmbeddingsPageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
export type {CrA11yAnnouncerMessagesSentEvent} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
export {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
export {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
export {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
export {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
export {CrRouter} from 'chrome://resources/js/cr_router.js';
export {getTrustedHTML} from 'chrome://resources/js/static_types.js';
export {HistoryAppElement} from './app.js';
export type {BrowserProxy, RemoveVisitsRequest} from './browser_proxy.js';
export {BrowserProxyImpl} from './browser_proxy.js';
export {HistoryPageViewHistogram, HistorySignInState, SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram, SyncState, VisitContextMenuAction} from './constants.js';
export type {ForeignSession, ForeignSessionTab, ForeignSessionWindow, HistoryIdentityState} from './externs.js';
// <if expr="not is_chromeos">
export {HistoryCrossDeviceSigninPromoElement} from './history_cross_device_signin_promo.js';
export {HistoryCrossDeviceSigninPromoBrowserProxy} from './history_cross_device_signin_promo_browser_proxy.js';
// </if>
export {HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY, HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY, HistoryEmbeddingsPromoElement} from './history_embeddings_promo.js';
export {HistoryFilterChipsElement} from './history_filter_chips.js';
export {HistoryItemElement} from './history_item.js';
export type {ActionMenuModel} from './history_list.js';
export {HistoryListElement} from './history_list.js';
export {HistoryToolbarElement} from './history_toolbar.js';
export type {ChangeQueryEvent} from './query_manager.js';
export {HistorySearchedLabelElement} from './searched_label.js';
export {HistorySideBarElement} from './side_bar.js';
export {HistorySyncedDeviceCardElement} from './synced_device_card.js';
export {HistorySyncedDeviceManagerElement} from './synced_device_manager.js';
