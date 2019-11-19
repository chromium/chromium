// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * utils for machine_learning_internals.js
 */

cr.define('machine_learning_internals.utils', function() {
  'use strict';

  /**
   * Convert an enumValue to its name.
   * @param {number} enumValue An enum variable which denotes the result of some
   *     operation like LoadModel, CreateGraphExecutor or Execute.
   * @param {!Object} enumType The enum type which defines the enumValue.
   * @return {string} Name of the enum, like OK or ERROR.
   */
  function enumToString(enumValue, enumType) {
    for (let key in enumType) {
      if (enumValue === enumType[key]) {
        return key;
      }
    }
    assertNotReached('Unknown enumValue: ' + enumValue);
  }

  /**
   * @typedef {{
   *   data: chromeos.machineLearning.mojom.ValueList,
   *   shape: {value: !Array<number>},
   * }}
   */
  let TensorComponent;

  /**
   * Make tensor from given valueList and shape.
   * @param {!Array<number>} valueList Array<float> that will be encapsuled in a
   *     Tensor
   * @param {!Array<number>} shape Shape of valueList. Product of its elements
   *     should equal to the length of valueList.
   * @return {machine_learning_internals.utils.TensorComponent} Tensor that
   *     contains shape and content of valueList.
   */
  function makeTensor(valueList, shape) {
    const expectedLength = shape.reduce((a, b) => a * b);
    if (expectedLength != valueList.length) {
      throw new Error(`valueList.length ${valueList.length} != expectedLength ${
          expectedLength}`);
    }
    return {
      shape: {
        value: shape,
      },
      data: {
        floatList: {value: valueList},
      },
    };
  }

  return {
    enumToString: enumToString,
    makeTensor: makeTensor,
    TensorComponent: TensorComponent,
  };
});
