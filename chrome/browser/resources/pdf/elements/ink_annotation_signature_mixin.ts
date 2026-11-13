// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ColorOption} from '../constants.js';
import {TEXT_AND_SIGNATURE_COLORS} from '../constants.js';

type Constructor<T> = new (...args: any[]) => T;

export const SIGNATURE_COLORS: ColorOption[] = TEXT_AND_SIGNATURE_COLORS;

export const InkAnnotationSignatureMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<InkAnnotationSignatureMixinInterface> => {
      class InkAnnotationSignatureMixin extends superClass implements
          InkAnnotationSignatureMixinInterface {
        static get properties() {
          return {
            colors: {type: Array},
          };
        }

        accessor colors: ColorOption[] = SIGNATURE_COLORS;
      }
      return InkAnnotationSignatureMixin;
    };

export interface InkAnnotationSignatureMixinInterface {
  colors: ColorOption[];
}
