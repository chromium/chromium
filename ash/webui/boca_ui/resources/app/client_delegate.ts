// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Value} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import type {Assignment as AssignmentMojom, Config, ControlledTab as ControlledTabMojom, Course, IdentifiedActivity as Activity, Identity as IdentityMojom, Material as MaterialMojom, NetworkInfo as NetworkInfoMojom, PageHandlerRemote, TabInfo, Window} from '../mojom/boca.mojom-webui.js';
import {CrdConnectionState as CrdConnectionStateMojom, CreateSessionError, SpeechRecognitionInstallState as SpeechRecognitionInstallStateMojom, SubmitAccessCodeError} from '../mojom/boca.mojom-webui.js';

import {BocaValidPref, CaptionConfig, ClientApiDelegate, ControlledTab, CrdConnectionState, CreateSessionResult, IdentifiedActivity, Identity, NetworkInfo, OnTaskConfig, Permission, PermissionSetting, SessionConfig, SpeechRecognitionInstallState, SubmitAccessCodeResult} from './boca_app.js';


const MICRO_SECS_IN_MINUTES: bigint = 60000000n;

function resultHasError(result: any) {
  return !(result.error === undefined || result.error === null);
}

export function getSpeechRecognitionInstallStateMojomToUI(
    state: SpeechRecognitionInstallStateMojom) {
  switch (state) {
    case SpeechRecognitionInstallStateMojom.kUnknown:
      return SpeechRecognitionInstallState.UNKNOWN;
    case SpeechRecognitionInstallStateMojom.kSystemLanguageUnsupported:
      return SpeechRecognitionInstallState.SYSTEM_LANGUAGE_NOT_SUPPORTED;
    case SpeechRecognitionInstallStateMojom.kInProgress:
      return SpeechRecognitionInstallState.IN_PROGRESS;
    case SpeechRecognitionInstallStateMojom.kFailed:
      return SpeechRecognitionInstallState.FAILED;
    case SpeechRecognitionInstallStateMojom.kReady:
      return SpeechRecognitionInstallState.READY;
    default:
      return SpeechRecognitionInstallState.UNKNOWN;
  }
}

export function getCrdConnectionStateMojomToUI(state: CrdConnectionStateMojom) {
  switch (state) {
    case CrdConnectionStateMojom.kUnknown:
      return CrdConnectionState.UNKNOWN;
    case CrdConnectionStateMojom.kConnecting:
      return CrdConnectionState.CONNECTING;
    case CrdConnectionStateMojom.kConnected:
      return CrdConnectionState.CONNECTED;
    case CrdConnectionStateMojom.kDisconnected:
      return CrdConnectionState.DISCONNECTED;
    default:
      return CrdConnectionState.UNKNOWN;
  }
}

export function getStudentActivityMojomToUI(activities: Activity[]):
    IdentifiedActivity[] {
  return activities.map((item: Activity) => {
    return {
      id: item.id,
      studentActivity: {
        studentStatusDetail: item.activity.studentStatusDetail.valueOf(),
        isActive: item.activity.isActive,
        activeTab: item.activity.activeTab ? item.activity.activeTab :
                                             undefined,
        isCaptionEnabled: item.activity.isCaptionEnabled,
        isHandRaised: item.activity.isHandRaised,
        joinMethod: item.activity.joinMethod.valueOf(),
        viewScreenSessionCode: item.activity.viewScreenSessionCode ?
            item.activity.viewScreenSessionCode :
            undefined,
      },
    };
  });
}
export function getSessionConfigMojomToUI(session: Config|
                                          undefined): SessionConfig|null {
  if (!session) {
    return null;
  }
  return {
    sessionDurationInMinutes:
        Number(session.sessionDuration.microseconds / MICRO_SECS_IN_MINUTES),
    sessionStartTime: session.sessionStartTime || undefined,
    teacher: session.teacher ? {
      id: session.teacher.id,
      name: session.teacher.name,
      email: session.teacher.email,
      photoUrl: session.teacher.photoUrl ? session.teacher.photoUrl.url :
                                           undefined,
    } :
                               undefined,
    students:
        session.students.map((item: IdentityMojom) => {
          return {
            id: item.id,
            name: item.name,
            email: item.email,
            photoUrl: item.photoUrl ? item.photoUrl.url : undefined,
          };
        }),
    studentsJoinViaCode:
        session.studentsJoinViaCode.map(
            (item: IdentityMojom) => {
              return {
                id: item.id,
                name: item.name,
                email: item.email,
                photoUrl: item.photoUrl ? item.photoUrl.url : undefined,
              };
            }),
    onTaskConfig: {
      isLocked: session.onTaskConfig.isLocked,
      isPaused: session.onTaskConfig.isPaused,
      tabs: session.onTaskConfig.tabs.map((item: ControlledTabMojom) => {
        return {
          tab: {
            id: item.tab.id ? item.tab.id : undefined,
            url: item.tab.url.url,
            title: item.tab.title,
            favicon: item.tab.favicon.url,
          },
          navigationType: item.navigationType.valueOf(),
        };
      }),
    },
    captionConfig: session.captionConfig,
    accessCode: session.accessCode ? session.accessCode : '',
  };
}

export function getNetworkInfoMojomToUI(networks: NetworkInfoMojom[]):
    NetworkInfo[] {
  return networks.map((item: NetworkInfoMojom) => ({
                        networkState: item.state.valueOf(),
                        networkType: item.type.valueOf(),
                        name: item.name,
                        signalStrength: item.signalStrength,
                      }));
}

/**
 * A delegate implementation that provides API via privileged mojom API
 */
export class ClientDelegateFactory {
  private clientDelegateImpl: ClientApiDelegate;
  constructor(pageHandler: PageHandlerRemote) {
    this.clientDelegateImpl = {
      authenticateWebview: async () => {
        return (await pageHandler.authenticateWebview()).success;
      },
      getWindowsTabsList: async () => {
        const result = await pageHandler.getWindowsTabsList();
        return result.windowList.map((window: Window) => {
          return {
            windowName: window.name ?? '',
            tabList: window.tabList.map((tab: TabInfo) => {
              return {
                id: tab.id ? tab.id : undefined,
                title: tab.title,
                url: tab.url.url,
                favicon: tab.favicon.url,
              };
            }),
          };
        });
      },
      getCourseList: async () => {
        const result = await pageHandler.listCourses();
        return result.courses.map((course: Course) => {
          return {
            id: course.id,
            name: course.name,
            section: course.section,
          };
        });
      },
      getStudentList: async (id: string) => {
        const result = await pageHandler.listStudents(id);
        return result.students.map((student: IdentityMojom) => {
          return {
            id: student.id,
            name: student.name,
            email: student.email,
            photoUrl: student.photoUrl ? student.photoUrl.url : undefined,
          };
        });
      },
      getAssignmentList: async (id: string) => {
        const result = await pageHandler.listAssignments(id);
        return result.assignments.map((assignment: AssignmentMojom) => {
          return {
            title: assignment.title,
            url: assignment.url.url,
            lastUpdateTime: assignment.lastUpdateTime,
            materials: assignment.materials.map((material: MaterialMojom) => {
              return {title: material.title, type: material.type.valueOf()};
            }),
            type: assignment.type.valueOf(),
          };
        });
      },
      createSession: async (sessionConfig: SessionConfig) => {
        const result = await pageHandler.createSession({
          sessionDuration: {
            microseconds: BigInt(sessionConfig.sessionDurationInMinutes) *
                MICRO_SECS_IN_MINUTES,
          },
          studentsJoinViaCode: [],
          teacher: null,
          accessCode: null,
          sessionStartTime: null,
          students: sessionConfig.students?.map((item: Identity) => {
            return {
              id: item.id,
              name: item.name,
              email: item.email,
              photoUrl: item.photoUrl ? {url: item.photoUrl} : null,
            };
          }),
          onTaskConfig: {
            isLocked: sessionConfig.onTaskConfig?.isLocked,
            isPaused: sessionConfig.onTaskConfig?.isPaused,
            tabs:
                sessionConfig.onTaskConfig?.tabs.map((item: ControlledTab) => {
                  return {
                    tab: {
                      id: null,
                      url: {url: item.tab.url},
                      title: item.tab.title,
                      favicon: {url: item.tab.favicon},
                    },
                    navigationType: item.navigationType.valueOf(),
                  };
                }),
          },
          captionConfig: sessionConfig.captionConfig,
        } as Config);
        if (!resultHasError(result)) {
          return CreateSessionResult.SUCCESS;
        } else if (result.error == CreateSessionError.kHTTPError) {
          return CreateSessionResult.HTTP_ERROR;
        } else if (result.error == CreateSessionError.kNetworkRestriction) {
          return CreateSessionResult.NETWORK_RESTRICTION;
        }
        return CreateSessionResult.SUCCESS;
      },
      getSession: async () => {
        const result = (await pageHandler.getSession()).result;
        if (!result.session) {
          return null;
        }
        return {
          sessionConfig: getSessionConfigMojomToUI(result.session.config) as
              SessionConfig,
          activity: getStudentActivityMojomToUI(result.session.activities),
        };
      },
      endSession: async () => {
        const result = await pageHandler.endSession();
        return !resultHasError(result);
      },
      extendSessionDuration: async (extendDurationInMinutes: number) => {
        const result = await pageHandler.extendSessionDuration({
          microseconds: BigInt(extendDurationInMinutes) * MICRO_SECS_IN_MINUTES
        });
        return !resultHasError(result);
      },
      removeStudent: async (id: string) => {
        const result = await pageHandler.removeStudent(id);
        return !resultHasError(result);
      },
      addStudents: async (students: Identity[]) => {
        const result =
            await pageHandler.addStudents(students?.map((item: Identity) => {
              return {
                id: item.id,
                name: item.name,
                email: item.email,
                photoUrl: item.photoUrl ? {url: item.photoUrl} : null,
              };
            }));
        return !resultHasError(result);
      },
      updateOnTaskConfig: async (onTaskConfig: OnTaskConfig) => {
        const result = await pageHandler.updateOnTaskConfig(
            {
              isLocked: onTaskConfig.isLocked,
              isPaused: onTaskConfig.isPaused ? onTaskConfig.isPaused : false,
              tabs: onTaskConfig.tabs.map((item: ControlledTab) => {
                return {
                  tab: {
                    id: null,
                    url: {url: item.tab.url},
                    title: item.tab.title,
                    favicon: {url: item.tab.favicon},
                  },
                  navigationType: item.navigationType.valueOf(),
                };
              }),
            },
        );
        return !resultHasError(result);
      },
      updateCaptionConfig: async (captionConfig: CaptionConfig) => {
        const result = await pageHandler.updateCaptionConfig(
            captionConfig,
        );
        return !resultHasError(result);
      },
      setFloatMode: async (isFloatMode: boolean) => {
        return (await pageHandler.setFloatMode(isFloatMode)).success;
      },
      submitAccessCode: async (code: string) => {
        const result = await pageHandler.submitAccessCode(code);
        if (!resultHasError(result)) {
          return SubmitAccessCodeResult.SUCCESS;
        } else if (result.error == SubmitAccessCodeError.kInvalid) {
          return SubmitAccessCodeResult.INVALID_CODE;
        } else if (result.error == SubmitAccessCodeError.kNetworkRestriction) {
          return SubmitAccessCodeResult.NETWORK_RESTRICTION;
        }
        return SubmitAccessCodeResult.SUCCESS;
      },
      viewStudentScreen: async (id: string) => {
        const result = await pageHandler.viewStudentScreen(id);
        return !resultHasError(result);
      },
      endViewScreenSession: async (id: string) => {
        const result = await pageHandler.endViewScreenSession(id);
        return !resultHasError(result);
      },
      setViewScreenSessionActive: async (id: string) => {
        const result = await pageHandler.setViewScreenSessionActive(id);
        return !resultHasError(result);
      },
      getUserPref: async (pref: BocaValidPref) => {
        return (await pageHandler.getUserPref(pref.valueOf())).value;
      },
      setUserPref: async (pref: BocaValidPref, value: Value) => {
        await pageHandler.setUserPref(pref.valueOf(), value);
      },
      setSitePermission: async (
          url: string, permission: Permission, setting: PermissionSetting) => {
        return (await pageHandler.setSitePermission(
                    url, permission.valueOf(), setting.valueOf()))
            .success;
      },
      closeTab: async (tabId: number) => {
        return (await pageHandler.closeTab(tabId)).success;
      },
      openFeedbackDialog: async () => {
        await pageHandler.openFeedbackDialog();
      },
      refreshWorkbook: async () => {
        await pageHandler.refreshWorkbook();
      },
      getSpeechRecognitionInstallationStatus: async () => {
        return getSpeechRecognitionInstallStateMojomToUI(
            (await pageHandler.getSpeechRecognitionInstallationStatus()).state);
      },
      renotifyStudent: async (id: string) => {
        const result = await pageHandler.renotifyStudent(id);
        return !resultHasError(result);
      },
      startSpotlight: async (crdConnectionCode: string) => {
        return await pageHandler.startSpotlight(crdConnectionCode);
      },
    };
  }

  getInstance(): ClientApiDelegate {
    return this.clientDelegateImpl;
  }
}
