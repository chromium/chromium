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
  id?: number;
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
  photoUrl?: string;
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
 * Declare a classroom course assignment information
 */
export declare interface Assignment {
  title: string;
  url: string;
  lastUpdateTime: Date;
  materials: Material[];
  type: AssignmentType;
}

/**
 * Declare an assignment material information
 */
export declare interface Material {
  title: string;
  type: MaterialType;
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
  SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED = 5
}

export enum JoinMethod {
  ROSTER = 0,
  ACCESS_CODE = 1,
}

export enum SubmitAccessCodeResult {
  UNKNOWN = 0,
  SUCCESS = 1,
  INVALID_CODE = 2,
}

/**
 * Declare network state enum type
 */
export enum NetworkState {
  ONLINE = 0,
  CONNECTED = 1,
  PORTAL = 2,
  CONNECTING = 3,
  NOTCONNECTED = 4,
}

/**
 * Declare network type enum type
 */
export enum NetworkType {
  CELLULAR = 0,
  ETHERNET = 1,
  WIFI = 2,
  UNSUPPORTED = 3,
}

/**
 * Declare permission type enum type
 */
export enum Permission {
  MICROPHONE = 0,
  CAMERA = 1,
}

/**
 * Declare permission setting type enum type
 */
export enum PermissionSetting {
  ALLOW = 0,
  ASK = 1,
  BLOCK = 2,
}

/**
 * Declare boca user pref type.
 */
export enum BocaValidPref {
  NAVIGATION_SETTING = 0,
  CAPTION_ENABLEMENT_SETTING = 1,
  DEFAULT_MEDIASTREAM_SETTING = 2,
}

/**
 * Declare course assignment material type enum type
 */
export enum MaterialType {
  UNKNOWN = 0,
  SHARED_DRIVE_FILE = 1,
  YOUTUBE_VIDEO = 2,
  LINK = 3,
  FORM = 4,
}

/**
 * Declare course assignment type enum type
 */
export enum AssignmentType {
  UNSPECIFIED = 0,
  ASSIGNMENT = 1,
  SHORT_ANSWER_QUESTION = 2,
  MULTIPLE_CHOICE_QUESTION = 3,
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
  sessionCaptionEnabled: boolean;
  localCaptionEnabled: boolean;
  sessionTranslationEnabled: boolean;
}

/**
 * Declare SessionConfig
 */
export declare interface SessionConfig {
  sessionStartTime?: Date;
  sessionDurationInMinutes: number;
  students: Identity[];
  studentsJoinViaCode?: Identity[];
  teacher?: Identity;
  onTaskConfig: OnTaskConfig;
  captionConfig: CaptionConfig;
  accessCode?: string;
}

/**
 * Declare Session
 */
export declare interface Session {
  sessionConfig: SessionConfig;
  activity: IdentifiedActivity[];
}

/**
 * Declare StudentActivity
 */
export declare interface StudentActivity {
  // Whether the student status have flipped from added to active in the
  // session.
  isActive: boolean;
  activeTab?: string;
  isCaptionEnabled: boolean;
  isHandRaised: boolean;
  // TODO(b/365191878): Remove this after refactoring existing schema to support
  // multi-group.
  joinMethod: JoinMethod;
  viewScreenSessionCode?: string;
}

/**
 * Declare IdentifiedActivity
 */
export declare interface IdentifiedActivity {
  id: string;
  studentActivity: StudentActivity;
}

/**
 * Declare NetworkInfo
 */
export declare interface NetworkInfo {
  networkState: NetworkState;
  networkType: NetworkType;
  name: string;
  signalStrength: number;
}

/**
 * The delegate which exposes privileged function to App
 */
export declare interface ClientApiDelegate {
  /**
   * Request authentication for the webview.
   */
  authenticateWebview(): Promise<boolean>;

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
   * Get list of assignments in a course.
   */
  getAssignmentList(courseId: string): Promise<Assignment[]>;

  /**
   * Create a new session.
   */
  createSession(sessionConfig: SessionConfig): Promise<boolean>;

  /**
   * Remove a student from the current session.
   */
  removeStudent(id: string): Promise<boolean>;

  /**
   * Retrivies the current session.
   */
  getSession(): Promise<Session|null>;
  /**
   * End the current session
   */
  endSession(): Promise<boolean>;

  /**
   * Extend session duration
   */
  extendSessionDuration(extendDurationInMinutes: number): Promise<boolean>;

  /**
   * Update on task config
   */
  updateOnTaskConfig(onTaskConfig: OnTaskConfig): Promise<boolean>;

  /**
   * Update caption config
   */
  updateCaptionConfig(captionConfig: CaptionConfig): Promise<boolean>;
  /**
   * Set float mode
   */
  setFloatMode(isFloatMode: boolean): Promise<boolean>;

  /**
   * Submit an access code for student to join the session.
   */
  submitAccessCode(accessCode: string): Promise<SubmitAccessCodeResult>;

  /**
   * Request to view the screen of the student with the given id.
   */
  viewStudentScreen(id: string): Promise<boolean>;

  /**
   * Request to end the view screen session of the student with the given id.
   */
  endViewScreenSession(id: string): Promise<boolean>;

  /**
   * Request to set the view screen session to active for the student with the
   * given id.
   */
  setViewScreenSessionActive(id: string): Promise<boolean>;

  /**
   * Get the value of a boca specific user pref.
   */
  getUserPref(pref: BocaValidPref): Promise<any>;

  /**
   * Set the value of a boca specific user pref.
   */
  setUserPref(pref: BocaValidPref, value: any): Promise<void>;

  /**
   * Set the permission of a site.
   */
  setSitePermission(
      url: string, permission: Permission,
      setting: PermissionSetting): Promise<boolean>;

  /**
   * Close the tab with tabId.
   */
  closeTab(tabId: number): Promise<boolean>;

  openFeedbackDialog(): Promise<void>;

  /**
   * Refresh the workbook for students.
   */
  refreshWorkbook(): Promise<void>;
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

  /**
   * Notify the app that the session config has been updated. Null if the
   * session has ended.
   */
  onSessionConfigUpdated(sessionConfig: SessionConfig|null): void;

  /**
   * Notify the app that the student activity has been updated.
   * The entire payload would be sent.
   */
  onStudentActivityUpdated(studentActivity: IdentifiedActivity[]): void;

  /**
   * Notify the app that the active networks has been updated.
   */
  onActiveNetworkStateChanged(activeNetworks: NetworkInfo[]): void;

  /**
   * Notify the app that the local captions has been turned off from the caption
   * bubble or by another mean from chrome. This can be called during a session
   * or outside of a session in the teacher case.
   */
  onLocalCaptionDisabled(): void;
}
