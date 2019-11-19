// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const BuiltinModelId = chromeos.machineLearning.mojom.BuiltinModelId;
const LoadModelResult = chromeos.machineLearning.mojom.LoadModelResult;
const CreateGraphExecutorResult =
    chromeos.machineLearning.mojom.CreateGraphExecutorResult;
const ExecuteResult = chromeos.machineLearning.mojom.ExecuteResult;

cr.ui.decorate('tabbox', cr.ui.TabBox);

cr.define('machine_learning_internals', function() {
  class BrowserProxy {
    constructor() {
      /**
       * @type {!chromeos.machineLearning.mojom.PageHandlerRemote}
       */
      this.pageHandler = chromeos.machineLearning.mojom.PageHandler.getRemote();
      /**
       * @private {!Map<BuiltinModelId,
       *     !chromeos.machineLearning.mojom.GraphExecutorRemote>}
       */
      this.modelMap_ = new Map();
    }

    /**
     * @param {!BuiltinModelId} modelId Model to load.
     * @return
     * {!Promise<!chromeos.machineLearning.mojom.GraphExecutorRemote>} A
     *     promise that resolves when loadBuiltinModel and createGraphExecutor
     *     both succeed.
     */
    async prepareModel(modelId) {
      if (this.modelMap_.has(modelId)) {
        return this.modelMap_.get(modelId);
      }
      const modelSpec = {id: modelId};
      /** @type {chromeos.machineLearning.mojom.ModelRemote} */
      const model = new chromeos.machineLearning.mojom.ModelRemote();
      const {result: loadModelResult} = await this.pageHandler.loadBuiltinModel(
          modelSpec, model.$.bindNewPipeAndPassReceiver());
      if (loadModelResult != LoadModelResult.OK) {
        const error = machine_learning_internals.utils.enumToString(
            loadModelResult, LoadModelResult);
        throw new Error(`LoadBuiltinModel failed! Error: ${error}.`);
      }
      /** @type {chromeos.machineLearning.mojom.GraphExecutorRemote} */
      const graphExecutor =
          new chromeos.machineLearning.mojom.GraphExecutorRemote();
      const {result: createGraphExecutorResult} =
          await model.createGraphExecutor(
              graphExecutor.$.bindNewPipeAndPassReceiver());
      if (createGraphExecutorResult != CreateGraphExecutorResult.OK) {
        const error = machine_learning_internals.utils.enumToString(
            createGraphExecutorResult, CreateGraphExecutorResult);
        throw new Error(`CreateGraphExecutor failed! Error: ${error}.`);
      }

      this.modelMap_.set(modelId, graphExecutor);
      return graphExecutor;
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
