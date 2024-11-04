// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';

// </if>


import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {AbslStatusCode} from '//resources/mojo/mojo/public/mojom/base/absl_status.mojom-webui.js';
import {CustomElement} from 'chrome-untrusted://resources/js/custom_element.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './data_sharing_app.html.js';
import type {DataSharingSdk, DataSharingSdkGetLinkParams, DataSharingSdkSitePreview} from './data_sharing_sdk_types.js';
import {Code, LearnMoreUrlType} from './data_sharing_sdk_types.js';

// Param names in loaded URL. Should match those in
// chrome/browser/ui/views/data_sharing/data_sharing_utils.cc.
enum UrlQueryParams {
  FLOW = 'flow',
  GROUP_ID = 'group_id',
  TOKEN_SECRET = 'token_secret',
  TAB_GROUP_ID = 'tab_group_id',
}

enum FlowValues {
  SHARE = 'share',
  JOIN = 'join',
  MANAGE = 'manage',
}

// TODO(crbug.com/376347328): Replace with real learn more urls.
const learnMoreUrlMap = {
  [LearnMoreUrlType.LEARN_MORE_URL_TYPE_UNSPECIFIED]: () =>
      'about:blank',
  [LearnMoreUrlType.PEOPLE_WITH_ACCESS_SUBTITLE]: () => 'about:blank',
  [LearnMoreUrlType.DESCRIPTION_INVITE]: () => 'about:blank',
  [LearnMoreUrlType.DESCRIPTION_JOIN]: () => 'about:blank',
  [LearnMoreUrlType.BLOCK]: () => 'about:blank',
};

export class DataSharingApp extends CustomElement {
  private initialized_: boolean = false;
  private dataSharingSdk_: DataSharingSdk =
      window.data_sharing_sdk.buildDataSharingSdk();
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  static get is() {
    return 'data-sharing-app';
  }

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.browserProxy_.callbackRouter.onAccessTokenFetched.addListener(
        (accessToken: string) => {
          this.dataSharingSdk_.setOauthAccessToken({accessToken});
          if (!this.initialized_) {
            this.processUrl();
            this.browserProxy_.showUi();
            this.initialized_ = true;
          }
        },
    );
  }

  connectedCallback() {
    ColorChangeUpdater.forDocument().start();
  }

  // Called with when the owner presses copy link in share dialog.
  private makeTabGroupShared(tabGroupId: string, groupId: string) {
    this.browserProxy_.handler!.associateTabGroupWithGroupId(
        tabGroupId, groupId);
  }

  private getShareLink(params: DataSharingSdkGetLinkParams): Promise<string> {
    return this.browserProxy_.handler!
        .getShareLink(params.groupId, params.tokenSecret!)
        .then(res => res.url.url);
  }

  private getTabGroupPreview(groupId: string, tokenSecret: string):
      Promise<DataSharingSdkSitePreview[]> {
    return new Promise((resolve) => {
      const previews: DataSharingSdkSitePreview[] = [];
      this.browserProxy_.handler!.getTabGroupPreview(groupId, tokenSecret)
          .then((res) => {
            if (res.groupPreview.statusCode !== AbslStatusCode.kOk) {
              // TODO(crbug.com/368634445): Ask Chrome to handle different
              // errors in addition to closing the WebUI.
              this.browserProxy_.handler!.closeUI(Code.UNKNOWN);
            }

            res.groupPreview.sharedTabs.map((sharedTab) => {
              previews.push({
                url: sharedTab.displayUrl,
                faviconUrl: sharedTab.faviconUrl.url,
              });
            });
            resolve(previews);
          });
    });
  }

  private processUrl() {
    const currentUrl = urlForTesting ? urlForTesting : window.location.href;
    const params = new URL(currentUrl).searchParams;
    const flow = params.get(UrlQueryParams.FLOW);
    const groupId = params.get(UrlQueryParams.GROUP_ID);
    const tokenSecret = params.get(UrlQueryParams.TOKEN_SECRET);
    const tabGroupId = params.get(UrlQueryParams.TAB_GROUP_ID);
    const parent = this.getRequiredElement('#dialog-container');

    switch (flow) {
      case FlowValues.SHARE:
        this.dataSharingSdk_
            .runInviteFlow({
              parent,
              getShareLink: (params: DataSharingSdkGetLinkParams):
                  Promise<string> => {
                    this.makeTabGroupShared(tabGroupId!, params.groupId);
                    return this.getShareLink(params);
                  },
              // TODO(crbug.com/376348102): Provide group name to share flow.
              groupName: '',
              learnMoreUrlMap,
            })
            .then((res) => {
              this.browserProxy_.closeUi(res.status);
            });
        break;
      case FlowValues.JOIN:
        // group_id and token_secret cannot be null for join flow.
        this.dataSharingSdk_
            .runJoinFlow({
              parent,
              groupId: groupId!,
              tokenSecret: tokenSecret!,
              learnMoreUrlMap: learnMoreUrlMap,
              onJoinSuccessful: () => {
                this.browserProxy_.handler!.openTabGroup(groupId!);
              },
              fetchPreviewData: () => {
                return this.getTabGroupPreview(groupId!, tokenSecret!);
              },
            })
            .then((res) => {
              this.browserProxy_.closeUi(res.status);
            });
        break;
      case FlowValues.MANAGE:
        // group_id cannot be null for manage flow.
        this.dataSharingSdk_
            .runManageFlow({
              parent,
              groupId: groupId!,
              getShareLink: (params: DataSharingSdkGetLinkParams):
                  Promise<string> => {
                    return this.getShareLink(params);
                  },
              learnMoreUrlMap,
            })
            .then((res) => {
              this.browserProxy_.closeUi(res.status);
            });
        break;
      default:
        break;
    }
  }

  static setUrlForTesting(url: string) {
    urlForTesting = url;
  }
}

let urlForTesting: string|null = null;

declare global {
  interface HTMLElementTagNameMap {
    'data-sharing-app': DataSharingApp;
  }
}

customElements.define(DataSharingApp.is, DataSharingApp);
