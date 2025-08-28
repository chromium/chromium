// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(`Tests Autofill.trigger with a credit card.`);

  async function getFormFieldValue(id) {
    return await session.evaluate(`document.getElementById("${id}").value`);
  }

  await session.navigate('/resources/autofill_creditcard_form.html');

  const {result: {result: {objectId: elementObjectId}}} =
      await dp.Runtime.evaluate(
          {expression: 'document.getElementById("CREDIT_CARD_NUMBER")'});

  const {result: {node: {backendNodeId: backendNodeId}}} =
      await dp.DOM.describeNode({objectId: elementObjectId});

  const CREDIT_CARD_DATA = {
    number: '4111111111111111',
    name: 'John Smith',
    expiryMonth: '12',
    expiryYear: '2030',
    cvc: '123',
  };

  testRunner.log(
      await dp.Autofill.trigger(
          {fieldId: backendNodeId, card: CREDIT_CARD_DATA}),
      '\n`Autofill.trigger` result: ');

  const form_data = {
    number: await getFormFieldValue('CREDIT_CARD_NUMBER'),
    name: await getFormFieldValue('CREDIT_CARD_NAME_FULL'),
    expiryMonth: await getFormFieldValue('CREDIT_CARD_EXP_MONTH'),
    expiryYear: await getFormFieldValue('CREDIT_CARD_EXP_4_DIGIT_YEAR'),
    cvc: await getFormFieldValue('CREDIT_CARD_CVC'),
  };

  testRunner.log(form_data, '\nForm data: ');

  testRunner.completeTest();
})
