// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Config, ControlledTab as ControlledTabMojom, Course, IdentifiedActivity as Activity, Identity as IdentityMojom, NetworkInfo as NetworkInfoMojom, PageHandlerRemote, TabInfo, Window} from '../mojom/boca.mojom-webui.js';

import {CaptionConfig, ClientApiDelegate, ControlledTab, IdentifiedActivity, Identity, NetworkInfo, OnTaskConfig, SessionConfig, SubmitAccessCodeResult} from './boca_app.js';


const MICRO_SECS_IN_MINUTES: bigint = 60000000n;

function resultHasError(result: any) {
  return !(result.error === undefined || result.error === null);
}

export function getStudentActivityMojomToUI(activities: Activity[]):
    IdentifiedActivity[] {
  return activities.map((item: Activity) => {
    return {
      id:
        item.id, studentActivity: {
          isActive: item.activity.isActive,
          activeTab: item.activity.activeTab ? item.activity.activeTab :
                                               undefined,
          isCaptionEnabled: item.activity.isCaptionEnabled,
          isHandRaised: item.activity.isHandRaised,
          joinMethod: item.activity.joinMethod.valueOf()
        }
    }
  })
}
export function getSessionConfigMojomToUI(session: Config|
                                          undefined): SessionConfig|null {
  if (!session) {
    return null;
  }
  return {
  sessionDurationInMinutes:
    Number(session.sessionDuration.microseconds / MICRO_SECS_IN_MINUTES),
        sessionStartTime: session.sessionStartTime?.msec ?
        new Date(session.sessionStartTime.msec) :
        undefined,
        teacher: session.teacher ? {
          id: session.teacher.id,
          name: session.teacher.name,
          email: session.teacher.email,
          photoUrl: session.teacher.photoUrl ? session.teacher.photoUrl.url :
                                               undefined,
        } :
                                   undefined,
        students: session.students.map((item: IdentityMojom) => {
          return {
            id: item.id,
            name: item.name,
            email: item.email,
            photoUrl: item.photoUrl ? item.photoUrl.url : undefined,
          };
        }),
        studentsJoinViaCode:
            session.studentsJoinViaCode.map((item: IdentityMojom) => {
              return {
                id: item.id,
                name: item.name,
                email: item.email,
                photoUrl: item.photoUrl ? item.photoUrl.url : undefined,
              };
            }),
        onTaskConfig: {
          isLocked: session.onTaskConfig.isLocked,
          tabs: session.onTaskConfig.tabs.map((item: ControlledTabMojom) => {
            return {
              tab: {
                url: item.tab.url.url,
                title: item.tab.title,
                favicon: item.tab.favicon,
              },
              navigationType: item.navigationType.valueOf(),
            };
          }),
        },
        captionConfig: session.captionConfig,
        accessCode: session.accessCode ? session.accessCode : ''
  }
};

export function getNetworkInfoMojomToUI(networks: NetworkInfoMojom[]):
    NetworkInfo[]{return networks.map((item: NetworkInfoMojom) => ({
                                        networkState: item.state.valueOf(),
                                        networkType: item.type.valueOf(),
                                        name: item.name,
                                        signalStrength: item.signalStrength
                                      }))};

/**
 * A delegate implementation that provides API via privileged mojom API
 */
export class ClientDelegateFactory {
  private clientDelegateImpl: ClientApiDelegate;
  constructor(pageHandler: PageHandlerRemote) {
    this.clientDelegateImpl = {
      getWindowsTabsList: async () => {
        const result = await pageHandler.getWindowsTabsList();
        return result.windowList.map((window: Window) => {
          return {
            windowName: window.name ?? '',
            tabList: window.tabList.map((tab: TabInfo) => {
              return {
                title: tab.title,
                url: tab.url.url,
                favicon: tab.favicon,
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
            // TODO(b/356706279): Add section data.
            section: '',
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
            tabs:
                sessionConfig.onTaskConfig?.tabs.map((item: ControlledTab) => {
                  return {
                    tab: {
                      url: {url: item.tab.url},
                      title: item.tab.title,
                      favicon: item.tab.favicon,
                    },
                    navigationType: item.navigationType.valueOf(),
                  };
                }),
          },
          captionConfig: sessionConfig.captionConfig,
        } as Config);
        return result.success;
      },
      getSession: async () => {
        const result = (await pageHandler.getSession()).result;
        if (!result.config) {
          return null;
        }
        return {
          sessionConfig: getSessionConfigMojomToUI(result.config) as
              SessionConfig,
          // TODO(b/365191878): Fill in user activity.
          activity: []
        };
      },
      endSession: async () => {
        const result = await pageHandler.endSession();
        return !resultHasError(result);
      },
      removeStudent: async (id: string) => {
        const result = await pageHandler.removeStudent(id);
        return !resultHasError(result);
      },
      updateOnTaskConfig: async (onTaskConfig: OnTaskConfig) => {
        const result = await pageHandler.updateOnTaskConfig(
            {
              isLocked: onTaskConfig.isLocked,
              tabs: onTaskConfig.tabs.map((item: ControlledTab) => {
                return {
                  tab: {
                    url: {url: item.tab.url},
                    title: item.tab.title,
                    favicon: item.tab.favicon,
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
        }
        return SubmitAccessCodeResult.INVALID_CODE;
      }
    };
  }

  getInstance(): ClientApiDelegate {
    return this.clientDelegateImpl;
  }
}
