// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin for policy controlled indicators.
 * TODO(michaelpg): Since extensions can also control settings and be indicated,
 * rework the "policy" naming scheme throughout this directory.
 * Forked from
 * ui/webui/resources/cr_elements/policy/cr_policy_indicator_mixin.ts
 */

import {assertNotReached} from '//resources/js/assert.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Strings required for policy indicators. These must be set at runtime.
 * Chrome OS only strings may be undefined.
 */
export interface CrPolicyStringsType {
  controlledSettingExtension: string;
  controlledSettingExtensionWithoutName: string;
  controlledSettingPolicy: string;
  controlledSettingRecommendedMatches: string;
  controlledSettingRecommendedDiffers: string;
  controlledSettingParent: string;
  controlledSettingChildRestriction: string;
  controlledSettingShared: string;
  controlledSettingWithOwner: string;
  controlledSettingNoOwner: string;
}

declare global {
  interface Window {
    CrPolicyStrings: Partial<CrPolicyStringsType>;
  }
}

/**
 * Possible policy indicators that can be shown in settings.
 */
export enum CrPolicyIndicatorType {
  DEVICE_POLICY = 'devicePolicy',
  EXTENSION = 'extension',
  NONE = 'none',
  OWNER = 'owner',
  PRIMARY_USER = 'primary_user',
  RECOMMENDED = 'recommended',
  USER_POLICY = 'userPolicy',
  PARENT = 'parent',
  CHILD_RESTRICTION = 'childRestriction',
}

type Constructor<T> = new (...args: any[]) => T;

export const CrPolicyIndicatorMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrPolicyIndicatorMixinInterface> => {
      class CrPolicyIndicatorMixin extends superClass implements
          CrPolicyIndicatorMixinInterface {
        // Properties exposed to all policy indicators.
        static get properties() {
          return {
            /**
             * Which indicator type to show (or NONE).
             */
            indicatorType: {
              type: String,
              value: CrPolicyIndicatorType.NONE,
            },

            /**
             * The name associated with the policy source. See
             * chrome.settingsPrivate.PrefObject.controlledByName.
             */
            indicatorSourceName: {
              type: String,
              value: '',
            },

            // Computed properties based on indicatorType and
            // indicatorSourceName. Override to provide different values.

            indicatorVisible: {
              type: Boolean,
              computed: 'getIndicatorVisible_(indicatorType)',
            },

            indicatorIcon: {
              type: String,
              computed: 'getIndicatorIcon_(indicatorType)',
            },
          };
        }

        indicatorType: CrPolicyIndicatorType;
        indicatorSourceName: string;
        indicatorVisible: boolean;
        indicatorIcon: string;

        /**
         * @return True if the indicator should be shown.
         */
        private getIndicatorVisible_(type: CrPolicyIndicatorType): boolean {
          return type !== CrPolicyIndicatorType.NONE;
        }

        /**
         * @return {string} The iron-icon icon name.
         */
        private getIndicatorIcon_(type: CrPolicyIndicatorType): string {
          switch (type) {
            case CrPolicyIndicatorType.EXTENSION:
              return 'cr:extension';
            case CrPolicyIndicatorType.NONE:
              return '';
            case CrPolicyIndicatorType.PRIMARY_USER:
              return 'cr:group';
            case CrPolicyIndicatorType.OWNER:
              return 'cr:person';
            case CrPolicyIndicatorType.USER_POLICY:
            case CrPolicyIndicatorType.DEVICE_POLICY:
            case CrPolicyIndicatorType.RECOMMENDED:
              return 'cr20:domain';
            case CrPolicyIndicatorType.PARENT:
            case CrPolicyIndicatorType.CHILD_RESTRICTION:
              return 'cr20:kite';
            default:
              assertNotReached();
          }
        }

        /**
         * @param name The name associated with the indicator. See
         *     chrome.settingsPrivate.PrefObject.controlledByName
         * @param matches For RECOMMENDED only, whether the indicator
         *     value matches the recommended value.
         * @return The tooltip text for |type|.
         */
        getIndicatorTooltip(
            type: CrPolicyIndicatorType, name: string,
            matches?: boolean): string {
          if (!window.CrPolicyStrings) {
            return '';
          }  // Tooltips may not be defined, e.g. in OOBE.

          const CrPolicyStrings = window.CrPolicyStrings;
          switch (type) {
            case CrPolicyIndicatorType.EXTENSION:
              return name.length > 0 ?
                  CrPolicyStrings.controlledSettingExtension!.replace(
                      '$1', name) :
                  CrPolicyStrings.controlledSettingExtensionWithoutName!;
            case CrPolicyIndicatorType.PRIMARY_USER:
              return CrPolicyStrings.controlledSettingShared!.replace(
                  '$1', name);
            case CrPolicyIndicatorType.OWNER:
              return name.length > 0 ?
                  CrPolicyStrings.controlledSettingWithOwner!.replace(
                      '$1', name) :
                  CrPolicyStrings.controlledSettingNoOwner!;
            case CrPolicyIndicatorType.USER_POLICY:
            case CrPolicyIndicatorType.DEVICE_POLICY:
              return CrPolicyStrings.controlledSettingPolicy!;
            case CrPolicyIndicatorType.RECOMMENDED:
              return matches ?
                  CrPolicyStrings.controlledSettingRecommendedMatches! :
                  CrPolicyStrings.controlledSettingRecommendedDiffers!;
            case CrPolicyIndicatorType.PARENT:
              return CrPolicyStrings.controlledSettingParent!;
            case CrPolicyIndicatorType.CHILD_RESTRICTION:
              return CrPolicyStrings.controlledSettingChildRestriction!;
          }
          return '';
        }
      }

      return CrPolicyIndicatorMixin;
    });

export interface CrPolicyIndicatorMixinInterface {
  indicatorType: CrPolicyIndicatorType;
  indicatorSourceName: string;
  indicatorVisible: boolean;
  indicatorIcon: string;

  getIndicatorTooltip(
      type: CrPolicyIndicatorType, name: string, matches?: boolean): string;
}
