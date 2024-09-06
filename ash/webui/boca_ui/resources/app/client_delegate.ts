// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Config, Course, Identity, PageHandlerRemote, TabInfo, Window} from '../mojom/boca.mojom-webui.js';

import {ClientApiDelegate, ControlledTab, SessionConfig} from './boca_app.js';

const MICRO_SECS_IN_MINUTES: bigint = 60000000n;

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
            section: 'default',
          };
        });
      },
      getStudentList: async (id: string) => {
        const result = await pageHandler.listStudents(id);
        return result.students.map((student: Identity) => {
          return {
            id: student.id,
            name: student.name,
            email: student.email,
          };
        });
      },
      createSession: async (sessionConfig: SessionConfig) => {
        console.log(
            'Actual session duration is:' +
            sessionConfig.sessionDurationInMinutes +
            ', but we overrided it to 2min');
        const result = await pageHandler.createSession({
          sessionDuration: {
            // TODO(b/365141108) Hardcode session duration to 2 mintes until we
            // have end session.
            microseconds: BigInt(2) * MICRO_SECS_IN_MINUTES,
            // microseconds: BigInt(sessionConfig.sessionDurationInMinutes) *
            //     MICRO_SECS_IN_MINUTES,
          },
          students: sessionConfig.students,
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
    };
  }

  getInstance(): ClientApiDelegate {
    return this.clientDelegateImpl;
  }
}
