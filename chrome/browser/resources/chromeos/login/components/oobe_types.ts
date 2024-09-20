// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file contains typedefs for chromeOS OOBE properties.
 */

export namespace OobeTypes {

  /**
   * ChromeOS OOBE language descriptor.
   */
  export interface LanguageDsc {
    code?: string;
    displayName?: string;
    nativeDisplayName?: string;
    optionGroupName?: string;
    selected: boolean;
    textDirection?: string;
    title: string;
    value: string;
  }

  /**
   * ChromeOS OOBE input method descriptor.
   */
  export interface InputMethodsDsc {
    optionGroupName?: string;
    selected: boolean;
    title: string;
    value: string;
  }

  /**
   * ChromeOS OOBE demo country descriptor.
   */
  export interface DemoCountryDsc {
    value: string;
    title: string;
    selected?: boolean;
  }

  /**
   * Timezone ID.
   */
  export type Timezone = string;

  /**
   * ChromeOS timezone descriptor.
   */
  export interface TimezoneDsc {
    value?: OobeTypes.Timezone;
    title?: string;
    selected?: boolean;
  }

  export interface OobeScreen {
    tag: string;
    id: string;
    condition?: string;
    extra_classes?: string[];
  }
  export interface ScreensList extends Array<OobeScreen>{}

  /**
   * OOBE configuration, allows automation during OOBE.
   * Keys are also listed in chrome/browser/ash/login/configuration_keys.h
   */
  export interface OobeConfiguration {
    language?: string;
    inputMethod?: string;
    welcomeNext?: boolean;
    enableDemoMode?: boolean;
    demoPreferencesNext?: boolean;
    networkSelectGuid?: string;
    networkOfflineDemo?: boolean;
    eulaAutoAccept?: boolean;
    eulaSendStatistics?: boolean;
    networkUseConnected?: boolean;
    arcTosAutoAccept?: boolean;
    networkConfig?: string;
  }

  /**
   * Parameters passed to show PIN setup screen
   */
  export interface PinSetupScreenParameters {
    authToken: string;
    isChildAccount: boolean;
    hasLoginSupport: boolean;
    usingPinAsMainSignInFactor: boolean;
  }

  /**
   * Configuration of the security token PIN dialog.
   */
  export interface SecurityTokenPinDialogParameters {
    enableUserInput: boolean;
    attemptsLeft: number;
    hasError: boolean;
    formattedError: string;
    formattedAttemptsLeft: string;
  }

  /**
   * Data type that is expected for each app that is shown on the RecommendApps
   * screen.
   */
  export interface RecommendedAppsOldExpectedAppData {
    icon: string;
    name: string;
    package_name: string;
  }

  /**
   * Data type that is expected for each app that is shown on the RecommendApps
   * screen.
   */
  export interface RecommendedAppsExpectedAppData {
    title: string;
    icon_url: string;
    category: string;
    description: string;
    content_rating: number;
    content_rating_icon: string;
    in_app_purchases: boolean;
    was_installed: boolean;
    contains_ads: boolean;
    package_name: string;
    optimized_for_chrome: boolean;
  }

  /**
   * Event sent from inner webview to enclosing Recommended apps screen.
   */
  export interface RecommendedAppsSelectionEventData {
    type?: string;
    numOfSelected: number;
  }

  /**
   * Fatal Error Codes from SignInFatalErrorScreen
   */
  export enum FatalErrorCode {
    UNKNOWN = 0,
    SCRAPED_PASSWORD_VERIFICATION_FAILURE = 1,
    INSECURE_CONTENT_BLOCKED = 2,
    MISSING_GAIA_INFO = 3,
    CUSTOM = 4,
  }

  /**
   * Screen steps used by EnterpriseEnrollmentElement. Defined here to
   * avoid circular dependencies since it is needed by cr_ui.js
   */
  export enum EnrollmentStep {
    LOADING = 'loading',
    SIGNIN = 'signin',
    WORKING = 'working',
    ATTRIBUTE_PROMPT = 'attribute-prompt',
    ERROR = 'error',
    SUCCESS = 'success',
    CHECKING = 'checking',
    TPM_CHECKING = 'tpm-checking',
    KIOSK_ENROLLMENT = 'kiosk-enrollment',
    ATTRIBUTE_PROMPT_ERROR = 'attribute-prompt-error',
  }

  /**
   * Bottom buttons type of GAIA dialog.
   */
  export enum GaiaDialogButtonsType {
    DEFAULT = 'default',
    ENTERPRISE_PREFERRED = 'enterprise-preferred',
    KIOSK_PREFERRED = 'kiosk-preferred',
  }

  /**
   * Type of license used for enrollment.
   * Numbers for supported licenses should be in sync with
   * `LicenseType` from enrollment_config.h.
   */
  export enum LicenseType {
    NONE = 0,
    ENTERPRISE = 1,
    EDUCATION = 2,
    KIOSK = 3,
  }

  /**
   * Verification figure for the Quick Start screen.
   */
  export interface QuickStartScreenFigureData {
    shape: number;
    color: number;
    digit: number;
  }

}
