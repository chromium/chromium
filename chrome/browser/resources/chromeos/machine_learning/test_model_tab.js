// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('machine_learning_internals', function() {
  /**
   * @param {number} x The first addend.
   * @param {number} y The second addend.
   * @return {!Map<string, machine_learning_internals.utils.TensorComponent>}
   *     Input tensor that contains x and y.
   */
  function makeInput(x, y) {
    return new Map([
      ['x', machine_learning_internals.utils.makeTensor([x], [1])],
      ['y', machine_learning_internals.utils.makeTensor([y], [1])],
    ]);
  }

  async function testExecute() {
    try {
      /**
       * @type {chromeos.machineLearning.mojom.GraphExecutorRemote}
       */
      const testModelGraphExecutor =
          await machine_learning_internals.BrowserProxy.getInstance()
              .prepareModel(BuiltinModelId.TEST_MODEL);
      $('test-model-status').textContent = 'Model loaded successfully!';

      $('test-status').textContent = '';
      $('test-output').textContent = '';
      const x = Number.parseFloat($('test-input-x').value);
      const y = Number.parseFloat($('test-input-y').value);
      if (Number.isNaN(x) || Number.isNaN(y)) {
        $('test-status').textContent = '"X" and "Y" should both be numbers';
        return;
      }
      const input = makeInput(x, y);
      const response = await testModelGraphExecutor.execute(input, ['z']);
      const outputArray = response.outputs[0].data.floatList.value;
      const executeResult = machine_learning_internals.utils.enumToString(
          response.result, ExecuteResult);
      $('test-status').textContent = `Execute Result is ${executeResult}.`;
      $('test-output').textContent = outputArray.toString();
    } catch (/** @type {Error} */ e) {
      alert(e);
    }
  }

  return {
    testExecute: testExecute,
  };
});

document.addEventListener('DOMContentLoaded', () => {
  $('test-execute')
      .addEventListener('click', machine_learning_internals.testExecute);
});
