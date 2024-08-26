// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @ fileoverview
 * Types for the ChromeOS Boca App. The same file is consumed by both
 * the chromium and internal toolchains.
 */

/**
 * Declare tab information
 */
export declare interface TabInfo {
  title: string;
  url: string;
  favicon: string;
}
/**
 * Declare a browser window information
 */
export declare interface DeviceWindow {
  windowName?: string|undefined;
  tabList: TabInfo[];
}

/**
 * Declare a student/teacher information
 */
export declare interface Identity {
  id: string;
  name: string;
  email: string;
}

/**
 * Declare a classroom course information
 */
export declare interface Course {
  id: string;
  name: string;
  // Classroom metadata, to be shown as metadata
  section: string;
}

/**
 * Declare navigation enum type
 */
export enum NavigationType {
  UNKNOWN = 0,
  OPEN = 1,
  BLOCK = 2,
  DOMAIN = 3,
  LIMITED = 4,
}

/**
 * Declare controlled tab
 */
export declare interface ControlledTab {
  navigationType: NavigationType;
  tab: TabInfo;
}

/**
 * Declare OnTaskConfig
 */
export declare interface OnTaskConfig {
  isLocked: boolean;
  tabs: ControlledTab[];
}

/**
 * Declare CaptionConfig
 */
export declare interface CaptionConfig {
  captionEnabled: boolean;
  localOnly: boolean;
  transcriptionEnabled: boolean;
}

/**
 * Declare SessionConfig
 */
export declare interface SessionConfig {
  sessionStartTime?: Date;
  sessionDurationInMinutes: number;
  students: Identity[];
  teacher?: Identity;
  onTaskConfig: OnTaskConfig;
  captionConfig: CaptionConfig;
}


/**
 * The delegate which exposes privileged function to App
 */
export declare interface ClientApiDelegate {
  /**
   * Get a list of Window tabs opened on device.
   */
  getWindowsTabsList(): Promise<DeviceWindow[]>;

  /**
   * Get course list from Classroom.
   */
  getCourseList(): Promise<Course[]>;

  /**
   * Get list of students in a course.
   */
  getStudentList(courseId: string): Promise<Identity[]>;

  /**
   * Create a new session
   */
  createSession(sessionConfig: SessionConfig): Promise<boolean>;
}

/**
 * The client Api for interfacting with boca app instance.
 */
export declare interface ClientApi {
  /**
   * Sets the delegate through which BocaApp will access rendering data from.
   * @param delegate
   */
  setDelegate(delegate: ClientApiDelegate|null): void;
}
