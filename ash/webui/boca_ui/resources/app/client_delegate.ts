// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Course, PageHandlerRemote, Student, TabInfo, Window} from '../mojom/boca.mojom-webui.js';

import {ClientApiDelegate} from './boca_app.js';


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
        return result.students.map((student: Student) => {
          return {
            id: student.profile.id,
            name: student.profile.name.fullName,
            email: student.profile.emailAddress,
          };
        });
      },
    };
  }

  getInstance(): ClientApiDelegate {
    return this.clientDelegateImpl;
  }
}
