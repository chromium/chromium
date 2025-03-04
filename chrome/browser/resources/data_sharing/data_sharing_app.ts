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
import {assert} from 'chrome-untrusted://resources/js/assert.js';
import {CustomElement} from 'chrome-untrusted://resources/js/custom_element.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './data_sharing_app.html.js';
import type {DataSharingSdk, DataSharingSdkGetLinkParams, DataSharingSdkSitePreview, DynamicMessageParams, Logger, LoggingEvent, TranslationMap} from './data_sharing_sdk_types.js';
import {Code, DataSharingMemberRoleEnum, DynamicMessageKey, LearnMoreUrlType, LoggingIntent, Progress, StaticMessageKey} from './data_sharing_sdk_types.js';

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

// Events that can be triggered within the DataSharing UI.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DataSharingIntentType)
enum DataSharingIntentType {
  UNKNOWN = 0,
  STOP_SHARING = 1,
  LEAVE_GROUP = 2,
  REMOVE_ACCESS = 3,
  UPDATE_ACCESS = 4,
  BLOCK_USER = 5,
  REMOVE_USER = 6,
  REMOVE_ACCESS_TOKEN = 7,
  ADD_ACCESS_TOKEN = 8,
  COPY_LINK = 9,
  BLOCK_AND_LEAVE = 10,
  OPEN_GROUP_DETAILS = 11,
  OPEN_LEARN_MORE_URL = 12,
  ACCEPT_JOIN_AND_OPEN = 13,
  ABANDON_JOIN = 14,
}
// LINT.ThenChange(tools/metrics/histograms/metadata/data_sharing/enums.xml:DataSharingIntentType)

enum ProgressType {
  UNKNOWN = 'Unknown',
  STARTED = 'Started',
  FAILED = 'Failed',
  SUCCEEDED = 'Succeeded',
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
      [StaticMessageKey.FAIL_TO_UPDATE_ACCESS]:
          loadTimeData.getString('somethingWrongBody'),
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
          loadTimeData.getString('groupFullBody'),
      [StaticMessageKey.ACTIVITY_LOGS]: loadTimeData.getString('activityLog'),
      [StaticMessageKey.YOUR_GROUP_IS_FULL_DESCRIPTION]:
          loadTimeData.getString('ownerCannotShare'),
      [StaticMessageKey.CLOSE_FLOW_HEADER]:
          loadTimeData.getString('deleteLastDialogHeader'),
      [StaticMessageKey.KEEP_GROUP]: loadTimeData.getString('keepGroup'),
      [StaticMessageKey.DELETE_GROUP]: loadTimeData.getString('deleteGroup'),
      [StaticMessageKey.DELETE_FLOW_HEADER]:
          loadTimeData.getString('deleteFlowHeader'),
      [StaticMessageKey.DELETE]: loadTimeData.getString('delete'),
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
      [DynamicMessageKey.GET_GROUP_PREVIEW_ARIA_LABEL]: (
          params: DynamicMessageParams,
          ) =>
          loadTimeData.getStringF(
              'getGroupPreviewAriaLabel', params.group.name),
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
              'blockDialogBody', params.displayedUser!.name!,
              params.displayedUser!.email!, getTabGroupName()),
      [DynamicMessageKey.GET_BLOCK_AND_LEAVE_DIALOG_CONTENT]:
          (params: DynamicMessageParams) => loadTimeData.getStringF(
              'blockLeaveDialogBody', getTabGroupName(),
              params.displayedUser!.name!, params.displayedUser!.email!),
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
      [DynamicMessageKey.GET_CLOSE_FLOW_DESCRIPTION_FIRST_PARAGRAPH]:
          (params: DynamicMessageParams) => {
            if (params.displayedUser!.name! === getGroupOwnerName(params)) {
              return loadTimeData.getStringF('ownerDeleteLastTimeBody');
            } else {
              return loadTimeData.getStringF('memberDeleteLastTimeBody');
            }
          },
      [DynamicMessageKey.GET_CLOSE_FLOW_DESCRIPTION_SECOND_PARAGRAPH]:
          (params: DynamicMessageParams) => {
            if (params.displayedUser!.name! === getGroupOwnerName(params)) {
              return loadTimeData.getStringF(
                  'ownerDeleteLastTimeBody2', getTabGroupName());
            } else {
              return '';
            }
          },
      [DynamicMessageKey.GET_DELETE_FLOW_DESCRIPTION_CONTENT]: () =>
          loadTimeData.getStringF(
              'deleteFlowDescriptionContent', getTabGroupName()),
    },
  };
}

const learnMoreUrlMap = {
  [LearnMoreUrlType.LEARN_MORE_URL_TYPE_UNSPECIFIED]: () =>
      loadTimeData.getStringF('dataSharingUrl'),
  [LearnMoreUrlType.PEOPLE_WITH_ACCESS_SUBTITLE]: () =>
      loadTimeData.getStringF('learnMoreSharedTabGroupPageUrl'),
  [LearnMoreUrlType.DESCRIPTION_INVITE]: () =>
      loadTimeData.getStringF('learnMoreSharedTabGroupPageUrl'),
  [LearnMoreUrlType.DESCRIPTION_JOIN]: () =>
      loadTimeData.getStringF('learnMoreSharedTabGroupPageUrl'),
  [LearnMoreUrlType.BLOCK]: () =>
      loadTimeData.getStringF('learnAboutBlockedAccountsUrl'),
};

export class DataSharingApp extends CustomElement implements Logger {
  private initialized_: boolean = false;
  private dataSharingSdk_: DataSharingSdk =
      window.data_sharing_sdk.buildDataSharingSdk();
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private translationMap_: TranslationMap = createTranslationMap();
  private abandonJoin_: boolean = false;
  private successfullyJoined_: boolean = false;
  private tabGroupId_: string|null = null;

  static get is() {
    return 'data-sharing-app';
  }

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.dataSharingSdk_.updateClearcut(
        {enabled: loadTimeData.getBoolean('metricsReportingEnabled')});
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

  // Logger implementation.
  onEvent(event: LoggingEvent) {
    const intentMetricName =
        'DataSharing.Intent.' + this.getProgressType(event.progress);
    chrome.metricsPrivate.recordEnumerationValue(
        intentMetricName, this.getDataSharingIntentType(event.intentType),
        Object.keys(DataSharingIntentType).length);

    if (event.intentType === LoggingIntent.ABANDON_JOIN) {
      this.abandonJoin_ = true;
    }

    if (event.intentType === LoggingIntent.STOP_SHARING) {
      assert(this.tabGroupId_);
      if (event.progress === Progress.STARTED) {
        this.aboutToUnShareTabGroup(this.tabGroupId_);
      } else if (event.progress === Progress.SUCCEEDED) {
        this.onTabGroupUnShareComplete(this.tabGroupId_);
      }
    }
  }

  setSuccessfullyJoinedForTesting() {
    this.successfullyJoined_ = true;
  }

  private getProgressType(progress: Progress): ProgressType {
    switch (progress) {
      case (Progress.STARTED):
        return ProgressType.STARTED;
      case (Progress.FAILED):
        return ProgressType.FAILED;
      case (Progress.SUCCEEDED):
        return ProgressType.SUCCEEDED;
    }

    return ProgressType.UNKNOWN;
  }

  private getDataSharingIntentType(intent: LoggingIntent):
      DataSharingIntentType {
    switch (intent) {
      case (LoggingIntent.STOP_SHARING):
        return DataSharingIntentType.STOP_SHARING;
      case (LoggingIntent.LEAVE_GROUP):
        return DataSharingIntentType.LEAVE_GROUP;
      case (LoggingIntent.REMOVE_ACCESS):
        return DataSharingIntentType.REMOVE_ACCESS;
      case (LoggingIntent.UPDATE_ACCESS):
        return DataSharingIntentType.UPDATE_ACCESS;
      case (LoggingIntent.BLOCK_USER):
        return DataSharingIntentType.BLOCK_USER;
      case (LoggingIntent.REMOVE_USER):
        return DataSharingIntentType.REMOVE_USER;
      case (LoggingIntent.REMOVE_ACCESS_TOKEN):
        return DataSharingIntentType.REMOVE_ACCESS_TOKEN;
      case (LoggingIntent.ADD_ACCESS_TOKEN):
        return DataSharingIntentType.ADD_ACCESS_TOKEN;
      case (LoggingIntent.COPY_LINK):
        return DataSharingIntentType.COPY_LINK;
      case (LoggingIntent.BLOCK_AND_LEAVE):
        return DataSharingIntentType.BLOCK_AND_LEAVE;
      case (LoggingIntent.OPEN_GROUP_DETAILS):
        return DataSharingIntentType.OPEN_GROUP_DETAILS;
      case (LoggingIntent.OPEN_LEARN_MORE_URL):
        return DataSharingIntentType.OPEN_LEARN_MORE_URL;
      case (LoggingIntent.ACCEPT_JOIN_AND_OPEN):
        return DataSharingIntentType.ACCEPT_JOIN_AND_OPEN;
      case (LoggingIntent.ABANDON_JOIN):
        return DataSharingIntentType.ABANDON_JOIN;
    }

    return DataSharingIntentType.UNKNOWN;
  }

  // Called with when the owner presses copy link in share dialog.
  private makeTabGroupShared(tabGroupId: string, groupId: string) {
    this.browserProxy_.handler!.associateTabGroupWithGroupId(
        tabGroupId, groupId);
  }

  private aboutToUnShareTabGroup(tabGroupId: string) {
    this.browserProxy_.handler!.aboutToUnShareTabGroup(tabGroupId);
  }

  private onTabGroupUnShareComplete(tabGroupId: string) {
    this.browserProxy_.handler!.onTabGroupUnShareComplete(tabGroupId);
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
                faviconUrl: this.getFaviconServiceUrl(sharedTab.faviconUrl.url)
                                .toString(),
              });
            });
            resolve(previews);
          });
    });
  }

  // TODO(crbug.com/392965221): Use function from icon.ts instead.
  private getFaviconServiceUrl(pageUrl: string): URL {
    const url: URL = new URL('chrome-untrusted://favicon2');
    url.searchParams.set('size', '16');
    url.searchParams.set('scaleFactor', '1x');
    url.searchParams.set('allowGoogleServerFallback', '1');
    url.searchParams.set('pageUrl', pageUrl);
    return url;
  }

  private processUrl() {
    const currentUrl = urlForTesting ? urlForTesting : window.location.href;
    const params = new URL(currentUrl).searchParams;
    const flow = params.get(UrlQueryParams.FLOW);
    const groupId = params.get(UrlQueryParams.GROUP_ID);
    const tokenSecret = params.get(UrlQueryParams.TOKEN_SECRET);
    const tabGroupId = params.get(UrlQueryParams.TAB_GROUP_ID);
    const parent = this.getRequiredElement('#dialog-container');

    this.tabGroupId_ = tabGroupId;

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
              logger: this,
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
                this.successfullyJoined_ = true;
                this.browserProxy_.handler!.openTabGroup(groupId!);
              },
              fetchPreviewData: () => {
                return this.getTabGroupPreview(groupId!, tokenSecret!);
              },
              logger: this,
            })
            .then((res) => {
              let code: Code = res.status;
              if (!this.successfullyJoined_ && !this.abandonJoin_) {
                // If user neither succesfully joined nor abandon join, there
                // must be an error.
                code = Code.UNKNOWN;
              }

              this.browserProxy_.closeUi(code);
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
              activityLogCallback: () => {
                window.open(
                    loadTimeData.getStringF('activityLogsUrl'), '_blank');
              },
              logger: this,
            })
            .then((res) => {
              // There's a bug in the SDK this is returning earlier than
              // onEvent. The setTimeout is needed to make sure onEvent is
              // called before the UI closes.
              // TODO(crbug.com/399961647): Remove setTimeout once the SDK is
              // fixed.
              setTimeout(() => {
                this.browserProxy_.closeUi(res.status);
              }, 100);
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
