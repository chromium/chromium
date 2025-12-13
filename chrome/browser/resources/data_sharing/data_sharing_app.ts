// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// <if expr="_google_chrome">
import './data_sharing_sdk.js';

import {SHAREKIT_SDK_VERSION} from './data_sharing_sdk_version.js';
// </if>
// <if expr="not _google_chrome">
import {SHAREKIT_SDK_VERSION} from './dummy_data_sharing_sdk.js';
// </if>
// clang-format on



import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome-untrusted://resources/js/assert.js';
import {CustomElement} from 'chrome-untrusted://resources/js/custom_element.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {GroupAction, GroupActionProgress} from './data_sharing.mojom-webui.js';
import {getTemplate} from './data_sharing_app.html.js';
import type {DataSharingSdk, DataSharingSdkGetLinkParams, DynamicMessageParams, Logger, LoggingEvent, TranslationMap} from './data_sharing_sdk_types.js';
import {Code, DataSharingMemberRoleEnum, DynamicMessageKey, LearnMoreUrlType, LoggingIntent, Progress, StaticMessageKey} from './data_sharing_sdk_types.js';

// Param names in loaded URL. Should match those in
// chrome/browser/ui/views/data_sharing/data_sharing_utils.cc.
enum UrlQueryParams {
  FLOW = 'flow',
  GROUP_ID = 'group_id',
  TOKEN_SECRET = 'token_secret',
  TAB_GROUP_ID = 'tab_group_id',
  TAB_GROUP_TITLE = 'tab_group_title',
  IS_DISABLED_FOR_POLICY = 'is_disabled_for_policy',
}

enum FlowValues {
  SHARE = 'share',
  JOIN = 'join',
  MANAGE = 'manage',
  DELETE = 'delete',
  LEAVE = 'leave',
  CLOSE = 'close',
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

function toMojomGroupAction(intent: LoggingIntent): GroupAction {
  switch (intent) {
    case LoggingIntent.DELETE_GROUP:
      return GroupAction.kDeleteGroup;
    case LoggingIntent.LEAVE_GROUP:
      return GroupAction.kLeaveGroup;
    case LoggingIntent.ACCEPT_JOIN_AND_OPEN:
      return GroupAction.kJoinGroup;
    case LoggingIntent.STOP_SHARING:
      return GroupAction.kStopSharing;
    default:
      return GroupAction.kUnknown;
  }
}

function toMojomGroupActionProgress(progress: Progress): GroupActionProgress {
  switch (progress) {
    case Progress.STARTED:
      return GroupActionProgress.kStarted;
    case Progress.SUCCEEDED:
      return GroupActionProgress.kSuccess;
    case Progress.FAILED:
      return GroupActionProgress.kFailed;
    default:
      return GroupActionProgress.kUnknown;
  }
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
      [StaticMessageKey.SHARING_DISABLED_DESCRIPTION]:
          loadTimeData.getString('sharingDisabledDescription'),
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
              'getGroupPreviewAriaLabel', params.group.name) +
          ' ' +
          loadTimeData.getStringF(
              params.group.members.length <= 1 ? 'memberCountSingular' :
                                                 'memberCountPlural',
              params.group.members.length) +
          ' ' +
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
              params.displayedUser!.email!, getTabGroupName()),
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
            if (params.loggedInUser.role === DataSharingMemberRoleEnum.OWNER) {
              return loadTimeData.getStringF('ownerDeleteLastTimeBody');
            } else {
              return loadTimeData.getStringF('memberDeleteLastTimeBody');
            }
          },
      [DynamicMessageKey.GET_CLOSE_FLOW_DESCRIPTION_SECOND_PARAGRAPH]:
          (params: DynamicMessageParams) => {
            if (params.loggedInUser.role === DataSharingMemberRoleEnum.OWNER) {
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
  private convertedToSharedTabGroup: boolean = false;

  static get is() {
    return 'data-sharing-app';
  }

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.dataSharingSdk_.setClientVersionAndResetPeopleStore(
        loadTimeData.getStringF('currentClientVersion'),
        parseInt(SHAREKIT_SDK_VERSION));
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
        this.browserProxy_.aboutToUnShareTabGroup(this.tabGroupId_);
      } else if (event.progress === Progress.SUCCEEDED) {
        this.browserProxy_.onTabGroupUnShareComplete(this.tabGroupId_);
      }
    }

    this.browserProxy_.onGroupAction(
        toMojomGroupAction(event.intentType),
        toMojomGroupActionProgress(event.progress));
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
      default:
        break;
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
      default:
        break;
    }

    return DataSharingIntentType.UNKNOWN;
  }

  private processUrl() {
    const currentUrl = urlForTesting ? urlForTesting : window.location.href;
    const params = new URL(currentUrl).searchParams;
    const flow = params.get(UrlQueryParams.FLOW);
    const groupId = params.get(UrlQueryParams.GROUP_ID);
    const tokenSecret = params.get(UrlQueryParams.TOKEN_SECRET);
    const tabGroupId = params.get(UrlQueryParams.TAB_GROUP_ID);
    const parent = this.getRequiredElement('#dialog-container');
    const isSharingDisabled =
        (params.get(UrlQueryParams.IS_DISABLED_FOR_POLICY) === 'true');

    this.tabGroupId_ = tabGroupId;

    if (flow === FlowValues.SHARE) {
      parent.classList.add('invite');
    } else {
      parent.classList.remove('invite');
    }

    switch (flow) {
      case FlowValues.SHARE:
        document.title =
            loadTimeData.getStringF('shareGroupTitle', getTabGroupName());
        break;
      case FlowValues.MANAGE:
        document.title =
            loadTimeData.getStringF('manageGroupTitle', getTabGroupName());
        break;
      case FlowValues.JOIN:
        document.title = loadTimeData.getStringF('previewA11yName');
        break;
      default:
        break;
    }

    switch (flow) {
      case FlowValues.SHARE:
        this.dataSharingSdk_
            .runInviteFlow({
              parent,
              translatedMessages: this.translationMap_,
              getShareLink: async(params: DataSharingSdkGetLinkParams):
                  Promise<string> => {
                    // If the tab group is not shared before, go through
                    // makeTabGroupShared() Otherwise go through getShareLink().
                    if (!this.convertedToSharedTabGroup) {
                      const url = await this.browserProxy_.makeTabGroupShared(
                          tabGroupId!, params.groupId, params.tokenSecret!);
                      if (url === undefined) {
                        this.browserProxy_.closeUi(Code.UNKNOWN);
                        return '';
                      } else {
                        this.convertedToSharedTabGroup = true;
                        return url;
                      }
                    } else {
                      return this.browserProxy_.getShareLink(
                          params.groupId, params.tokenSecret!);
                    }
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
                // No need to return this promise since we want the bubble to
                // keep spinning until closed by the browser when new shared tab
                // group is received.
                return new Promise<void>(() => {});
              },
              fetchPreviewData: () => {
                return this.browserProxy_.getTabGroupPreview(
                    groupId!, tokenSecret!);
              },
              logger: this,
            })
            .then((res) => {
              if (this.successfullyJoined_) {
                // If user successfully joined, do nothing and keep the dialog
                // around until the tab group is delivered by sync server.
                return;
              }

              if (this.abandonJoin_) {
                this.browserProxy_.closeUi(res.status);
              } else {
                // If user neither successfully joined nor abandon join, there
                // must be an error.
                this.browserProxy_.closeUi(Code.UNKNOWN);
              }
            });
        break;
      case FlowValues.MANAGE:
      case FlowValues.LEAVE:
        // group_id cannot be null for manage flow.
        this.dataSharingSdk_
            .runManageFlow({
              parent,
              translatedMessages: this.translationMap_,
              groupId: groupId!,
              getShareLink: (params: DataSharingSdkGetLinkParams):
                  Promise<string> => {
                    return this.browserProxy_.getShareLink(
                        params.groupId, params.tokenSecret!);
                  },
              learnMoreUrlMap,
              activityLogCallback: () => {
                window.open(
                    loadTimeData.getStringF('activityLogsUrl'), '_blank');
              },
              logger: this,
              showLeaveDialogAtStartup: flow === FlowValues.LEAVE,
              isSharingDisabled,
            })
            .then((res) => {
              this.browserProxy_.closeUi(res.status);
            });
        break;
      case FlowValues.DELETE:
        // group_id cannot be null for delete flow.
        this.dataSharingSdk_
            .runDeleteFlow({
              parent,
              groupId: groupId!,
              translatedMessages: this.translationMap_,
              logger: this,
            })
            .then((res) => {
              this.browserProxy_.closeUi(res.status);
            });
        break;
      case FlowValues.CLOSE:
        // group_id cannot be null for close flow.
        this.dataSharingSdk_
            .runCloseFlow({
              parent,
              groupId: groupId!,
              translatedMessages: this.translationMap_,
              logger: this,
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
