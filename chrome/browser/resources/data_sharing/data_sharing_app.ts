// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';
// </if>

import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {AbslStatusCode} from '//resources/mojo/mojo/public/mojom/base/absl_status.mojom-webui.js';
import {CustomElement} from 'chrome-untrusted://resources/js/custom_element.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './data_sharing_app.html.js';
import type {DataSharingSdk, DataSharingSdkGetLinkParams, DataSharingSdkSitePreview, DynamicMessageParams, TranslationMap} from './data_sharing_sdk_types.js';
import {Code, DataSharingMemberRoleEnum, DynamicMessageKey, LearnMoreUrlType, StaticMessageKey} from './data_sharing_sdk_types.js';

// Param names in loaded URL. Should match those in
// chrome/browser/ui/views/data_sharing/data_sharing_utils.cc.
enum UrlQueryParams {
  FLOW = 'flow',
  GROUP_ID = 'group_id',
  TOKEN_SECRET = 'token_secret',
  TAB_GROUP_ID = 'tab_group_id',
  TAB_GROUP_TITLE = 'tab_group_title',
}

enum FlowValues {
  SHARE = 'share',
  JOIN = 'join',
  MANAGE = 'manage',
}

function getGroupOwnerName(params: DynamicMessageParams): string {
  for (const member of params.group.members) {
    if (member.role === DataSharingMemberRoleEnum.OWNER) {
      return member.name;
    }
  }
  return '';
}

function getGroupOwnerEmail(params: DynamicMessageParams): string {
  for (const member of params.group.members) {
    if (member.role === DataSharingMemberRoleEnum.OWNER) {
      return member.email;
    }
  }
  return '';
}

function getTabGroupName(): string {
  const currentUrl = urlForTesting ? urlForTesting : window.location.href;
  const params = new URL(currentUrl).searchParams;
  return params.get(UrlQueryParams.TAB_GROUP_TITLE) || '';
}

/** */
export function createTranslationMap(): TranslationMap {
  return {
    static: {
      [StaticMessageKey.CANCEL_LABEL]: loadTimeData.getString('cancel'),
      [StaticMessageKey.CANCEL]: loadTimeData.getString('cancel'),
      [StaticMessageKey.CLOSE]: loadTimeData.getString('close'),
      [StaticMessageKey.BACK]: loadTimeData.getString('back'),
      [StaticMessageKey.LOADING]: loadTimeData.getString('loading'),
      [StaticMessageKey.SOMETHING_WENT_WRONG]:
          loadTimeData.getString('somethingWrong'),
      [StaticMessageKey.THERE_WAS_AN_ERROR]:
          loadTimeData.getString('somethingWrongBody'),
      [StaticMessageKey.THERE_WAS_AN_ISSUE]:
          loadTimeData.getString('somethingWrongBody'),
      [StaticMessageKey.MORE_OPTIONS]: loadTimeData.getString('moreOptions'),
      [StaticMessageKey.MORE_OPTIONS_DESCRIPTION]:
          loadTimeData.getString('moreOptionsDescription'),
      /** Invite flow */
      [StaticMessageKey.COPY_LINK]: loadTimeData.getString('copyLink'),
      [StaticMessageKey.LINK_COPY_SUCCESS]:
          loadTimeData.getString('copyLinkSuccess'),
      [StaticMessageKey.LINK_COPY_FAILED]:
          loadTimeData.getString('copyLinkFailed'),
      /** Join flow */
      [StaticMessageKey.JOIN_AND_OPEN_LABEL]:
          loadTimeData.getString('previewDialogConfirm'),
      [StaticMessageKey.COLLECTION_LIST_TITLE]:
          loadTimeData.getString('tabsInGroup'),
      /** Manage flow */
      [StaticMessageKey.ANYONE_WITH_LINK_TOGGLE_TITLE]:
          loadTimeData.getString('linkJoinToggle'),
      [StaticMessageKey.ANYONE_WITH_LINK_TOGGLE_DESCRIPTION]:
          loadTimeData.getString('manageShareWisely'),
      [StaticMessageKey.BLOCK_AND_LEAVE_GROUP]:
          loadTimeData.getString('blockLeaveDialogTitle'),
      [StaticMessageKey.BLOCK_AND_LEAVE]:
          loadTimeData.getString('blockLeaveDialogConfrim'),
      [StaticMessageKey.LEARN_ABOUT_BLOCKED_ACCOUNTS]:
          loadTimeData.getString('blockLeaveLearnMore'),
      [StaticMessageKey.GOT_IT]: loadTimeData.getString('gotIt'),
      [StaticMessageKey.ABUSE_BANNER_CONTENT]:
          loadTimeData.getString('joinWarning'),
      [StaticMessageKey.STOP_SHARING_DIALOG_TITLE]:
          loadTimeData.getString('ownerStopSharingDialogTitle'),
      [StaticMessageKey.STOP_SHARING]:
          loadTimeData.getString('manageStopSharingOption'),
      [StaticMessageKey.BLOCK]: loadTimeData.getString('block'),
      [StaticMessageKey.LEAVE_GROUP]: loadTimeData.getString('leaveGroup'),
      [StaticMessageKey.LEAVE]: loadTimeData.getString('leaveGroupConfirm'),
      [StaticMessageKey.LEAVE_GROUP_DIALOG_TITLE]:
          loadTimeData.getString('leaveDialogTitle'),
      [StaticMessageKey.REMOVE]: loadTimeData.getString('remove'),
      [StaticMessageKey.YOU]: loadTimeData.getString('you'),
      [StaticMessageKey.OWNER]: loadTimeData.getString('owner'),
      /** Chrome Specific Content */
      [StaticMessageKey.INVITE_FLOW_DESCRIPTION_CONTENT]:
          loadTimeData.getString('shareGroupBody'),
      [StaticMessageKey.COPY_INVITE_LINK]:
          loadTimeData.getString('copyInviteLink'),
      [StaticMessageKey.LEARN_MORE_JOIN_FLOW]:
          loadTimeData.getString('learnMoreJoinFlow'),
      [StaticMessageKey.LEARN_ABOUT_SHARED_TAB_GROUPS]:
          loadTimeData.getString('learnMoreSharedTabGroup'),
      [StaticMessageKey.TAB_GROUP_DETAILS]:
          loadTimeData.getString('tabGroupDetailsTitle'),
      [StaticMessageKey.PEOPLE_WITH_ACCESS]:
          loadTimeData.getString('peopleWithAccess'),
      [StaticMessageKey.PEOPLE_WITH_ACCESS_SUBTITLE_MANAGE_FLOW]:
          loadTimeData.getString('peopleWithAccessSubtitleManageFlow'),
      [StaticMessageKey.ERROR_DIALOG_CONTENT]:
          loadTimeData.getString('errorDialogContent'),
      [StaticMessageKey.GROUP_FULL_TITLE]: loadTimeData.getString('groupFull'),
      [StaticMessageKey.GROUP_FULL_CONTENT]:
          loadTimeData.getString('ownerCannotShare'),
    },
    dynamic: {
      /** Invite flow */
      [DynamicMessageKey.GET_MEMBERSHIP_PREVIEW_OWNER_LABEL]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              'shareGroupShareAs', params.loggedInUser.name),
      /** Join flow */
      [DynamicMessageKey.GET_MEMBERSHIP_PREVIEW_INVITEE_LABEL]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF('joinGroupJoinAs', params.loggedInUser.name),
      [DynamicMessageKey.GET_GROUP_PREVIEW_MEMBER_DESCRIPTION]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              params.group.members.length <= 1 ? 'memberCountSingular' :
                                                 'memberCountPlural',
              params.group.members.length),
      [DynamicMessageKey.GET_GROUP_PREVIEW_TAB_DESCRIPTION]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              params.payload.mediaCount <= 1 ? 'tabCountSingular' :
                                               'tabCountPlural',
              params.payload.mediaCount),
      /** Manage flow */
      [DynamicMessageKey.GET_STOP_SHARING_DIALOG_CONTENT]: () =>
          loadTimeData.getStringF(
              'ownerStopSharingDialogBody', getTabGroupName()),
      [DynamicMessageKey.GET_REMOVE_DIALOG_TITLE]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              'ownerRemoveMemberDialogTitle', params.displayedUser!.name!),
      [DynamicMessageKey.GET_REMOVE_DIALOG_CONTENT]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              'ownerRemoveMemberDialogBody', params.displayedUser!.name!,
              params.displayedUser!.email!),
      [DynamicMessageKey.GET_LEAVE_GROUP_DIALOG_CONTENT]: () =>
          loadTimeData.getStringF('leaveDialogBody', getTabGroupName()),
      [DynamicMessageKey.GET_BLOCK_DIALOG_TITLE]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              'blockDialogTitle', params.displayedUser!.name!),
      [DynamicMessageKey.GET_BLOCK_DIALOG_CONTENT]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              'ownerRemoveMemberDialogBody', params.displayedUser!.name!,
              params.displayedUser!.email!, getTabGroupName()),
      [DynamicMessageKey.GET_BLOCK_AND_LEAVE_DIALOG_CONTENT]: () =>
          loadTimeData.getStringF('leaveDialogBody', getTabGroupName()),
      /** Chrome Specific Content */
      [DynamicMessageKey.GET_INVITE_FLOW_HEADER]: () =>
          loadTimeData.getStringF('shareGroupTitle', getTabGroupName()),
      [DynamicMessageKey.GET_JOIN_FLOW_DESCRIPTION_HEADER]:
          (params: DynamicMessageParams) => {
            const otherMemberCount = params.group.members.length - 1;
            return loadTimeData.getStringF(
                otherMemberCount <= 0 ?
                    'previewDialogTitleZero' :
                    (otherMemberCount === 1 ? 'previewDialogTitleSingular' :
                                              'previewDialogTitlePlural'),
                getGroupOwnerName(params), otherMemberCount);
          },
      [DynamicMessageKey.GET_JOIN_FLOW_DESCRIPTION_CONTENT]:
          (params: DynamicMessageParams) => loadTimeData.getStringF(
              'previewDialogBody', getGroupOwnerName(params),
              getGroupOwnerEmail(params)),
      [DynamicMessageKey.GET_MANAGE_FLOW_HEADER]: () =>
          loadTimeData.getStringF('manageGroupTitle', getTabGroupName()),
    },
  };
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
  private translationMap_: TranslationMap = createTranslationMap();

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

    if (flow === FlowValues.SHARE) {
      parent.classList.add('invite');
    } else {
      parent.classList.remove('invite');
    }

    switch (flow) {
      case FlowValues.SHARE:
        this.dataSharingSdk_
            .runInviteFlow({
              parent,
              translatedMessages: this.translationMap_,
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
              translatedMessages: this.translationMap_,
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
              translatedMessages: this.translationMap_,
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
