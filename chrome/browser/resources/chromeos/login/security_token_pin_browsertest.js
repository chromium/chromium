// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the <security-token-pin> Polymer element.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

GEN('#include "content/public/test/browser_test.h"');

var PolymerSecurityTokenPinTest = class extends Polymer2DeprecatedTest {
  /** @override */
  get browsePreload() {
    return 'chrome://oobe/login';
  }

  /** @override */
  setUp() {
    suiteSetup(async function() {
      console.warn('Running suite setup..');
      await cr.ui.Oobe.waitForOobeToLoad();
      console.warn('OOBE has been loaded. Continuing with test.');
    });
  }

  get extraLibraries() {
    return super.extraLibraries.concat(['components/oobe_types.js']);
  }
};

TEST_F('PolymerSecurityTokenPinTest', 'All', function() {
  const DEFAULT_PARAMETERS = {
    enableUserInput: true,
    hasError: false,
    formattedError: '',
    formattedAttemptsLeft: ''
  };

  let securityTokenPin;
  let pinKeyboardContainer;
  let pinKeyboard;
  let progressElement;
  let pinInput;
  let inputField;
  let errorContainer;
  let errorElement;
  let submitElement;
  let backElement;

  setup(() => {
    securityTokenPin = document.createElement('security-token-pin');
    document.body.appendChild(securityTokenPin);
    securityTokenPin.onBeforeShow();
    securityTokenPin.parameters = DEFAULT_PARAMETERS;

    pinKeyboardContainer = securityTokenPin.$$('#pinKeyboardContainer');
    assert(pinKeyboardContainer);
    pinKeyboard = securityTokenPin.$$('#pinKeyboard');
    assert(pinKeyboard);
    progressElement = securityTokenPin.$$('#progress');
    assert(progressElement);
    pinInput = pinKeyboard.$$('#pinInput');
    assert(pinInput);
    inputField = pinInput.$$('input');
    assert(inputField);
    errorContainer = securityTokenPin.$$('#errorContainer');
    assert(errorContainer);
    errorElement = securityTokenPin.$$('#error');
    assert(errorElement);
    submitElement = securityTokenPin.$$('#submit');
    assert(submitElement);
    backElement = securityTokenPin.$$('#back');
    assert(backElement);
  });

  teardown(() => {
    securityTokenPin.remove();
  });

  // Test that the 'completed' event is fired when the user submits the input.
  test('completion events in basic flow', () => {
    const FIRST_PIN = '0123';
    const SECOND_PIN = '987';

    let completedEventDetail = null;
    securityTokenPin.addEventListener('completed', (event) => {
      expectNotEquals(event.detail, null);
      expectEquals(completedEventDetail, null);
      completedEventDetail = event.detail;
    });
    securityTokenPin.addEventListener('cancel', () => {
      expectNotReached();
    });

    // The user enters some value. No 'completed' event is triggered so far.
    pinInput.value = FIRST_PIN;
    expectEquals(completedEventDetail, null);

    // The user submits the PIN. The 'completed' event has been triggered.
    submitElement.click();
    expectEquals(completedEventDetail, FIRST_PIN);
    completedEventDetail = null;

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };

    // The user enters some value. No new 'completed' event is triggered so far.
    pinInput.value = SECOND_PIN;
    expectEquals(completedEventDetail, null);

    // The user submits the new PIN. The 'completed' event has been triggered.
    submitElement.click();
    expectEquals(completedEventDetail, SECOND_PIN);
  });

  // Test that the input field accepts non-digit PIN.
  test('non-digit PIN input validity', () => {
    const NON_DIGIT_PIN = '+Aa';

    // The user enters a non-digit pin.
    pinInput.value = NON_DIGIT_PIN;

    expectEquals(pinInput.value, NON_DIGIT_PIN);
    expectEquals(inputField.value, NON_DIGIT_PIN);
  });

  // Test that the 'cancel' event is fired when the user aborts the dialog.
  test('completion events in cancellation flow', () => {
    let cancelEventCount = 0;
    securityTokenPin.addEventListener('cancel', () => {
      ++cancelEventCount;
    });
    securityTokenPin.addEventListener('completed', () => {
      expectNotReached();
    });

    // The user clicks the 'back' button. The cancel event is triggered.
    backElement.click();
    expectEquals(cancelEventCount, 1);
  });

  // Test that the submit button is only enabled when the input is non-empty.
  test('submit button availability', () => {
    // Initially, the submit button is disabled.
    expectTrue(submitElement.disabled);

    // The user enters a single digit. The submit button is enabled.
    pinInput.value = '1';
    expectFalse(submitElement.disabled);

    // The user clears the input. The submit button is disabled.
    pinInput.value = '';
    expectTrue(submitElement.disabled);
  });

  // Test that the input field is disabled when the final error is displayed and
  // no further user input is expected.
  test('input availability', () => {
    // Initially, the input is enabled.
    expectFalse(inputField.disabled);

    // The user enters and submits a PIN. The response arrives, requesting the
    // PIN again. The input is still enabled.
    pinInput.value = '123';
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    expectFalse(inputField.disabled);

    // The user enters and submits a PIN again. The response arrives, with a
    // final error. The input is disabled.
    pinInput.value = '456';
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: false,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    expectTrue(inputField.disabled);
  });

  // Test that the input field gets cleared when the user is prompted again.
  test('input cleared on new request', () => {
    const PIN = '123';
    pinInput.value = PIN;
    expectEquals(inputField.value, PIN);

    // The user submits the PIN. The response arrives, requesting the PIN again.
    // The input gets cleared.
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    expectEquals(pinInput.value, '');
    expectEquals(inputField.value, '');
  });

  // // Test that the input field gets cleared when the request fails with the
  // // final error.
  test('input cleared on final error', () => {
    // The user enters and submits a PIN. The response arrives, requesting the
    // PIN again. The input is cleared.
    const PIN = '123';
    pinInput.value = PIN;
    expectEquals(inputField.value, PIN);

    // The user submits the PIN. The response arrives, reporting a final error
    // and that the user input isn't requested anymore. The input gets cleared.
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: false,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    expectEquals(pinInput.value, '');
    expectEquals(inputField.value, '');
  });

  // Test that the PIN can be entered via the on-screen PIN keypad.
  test('PIN input via keypad', () => {
    const PIN = '13097';

    let completedEventDetail = null;
    securityTokenPin.addEventListener('completed', (event) => {
      completedEventDetail = event.detail;
    });

    // The user clicks the buttons of the on-screen keypad. The input field is
    // updated accordingly.
    for (const character of PIN)
      pinKeyboard.$$('#digitButton' + character).click();
    expectEquals(pinInput.value, PIN);
    expectEquals(inputField.value, PIN);

    // The user submits the PIN. The completed event is fired, containing the
    // PIN.
    submitElement.click();
    expectEquals(completedEventDetail, PIN);
  });

  // Test that the error is displayed only when it's set in the request.
  test('error visibility', () => {
    function getErrorContainerVisibility() {
      return getComputedStyle(errorContainer).getPropertyValue('visibility');
    }

    // Initially, no error is shown.
    expectEquals(getErrorContainerVisibility(), 'hidden');
    expectFalse(pinInput.hasAttribute('invalid'));

    // The user submits some PIN, and the error response arrives. The error gets
    // displayed.
    pinInput.value = '123';
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    expectEquals(getErrorContainerVisibility(), 'visible');
    expectTrue(pinInput.hasAttribute('invalid'));

    // The user modifies the input field. No error is shown.
    pinInput.value = '4';
    expectEquals(getErrorContainerVisibility(), 'hidden');
    expectFalse(pinInput.hasAttribute('invalid'));
  });

  // Test the text of the error label.
  test('label text: invalid PIN', () => {
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: 'Invalid PIN.',
      formattedAttemptsLeft: ''
    };
    expectEquals(errorElement.textContent, 'Invalid PIN.');
  });

  // Test the text of the error label when the user input is disabled.
  test('label text: max attempts exceeded', () => {
    securityTokenPin.parameters = {
      enableUserInput: false,
      hasError: true,
      formattedError: 'Maximum allowed attempts exceeded.',
      formattedAttemptsLeft: ''
    };
    expectEquals(
        errorElement.textContent, 'Maximum allowed attempts exceeded.');
  });

  // Test the text of the label when the number of attempts left is given.
  test('label text: attempts number', () => {
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: false,
      formattedError: '',
      formattedAttemptsLeft: '3 attempts left'
    };
    expectEquals(errorElement.textContent, '3 attempts left');
  });

  // Test that no scrolling is necessary in order to see all dots after entering
  // a PIN of a typical length.
  test('8-digit PIN fits into input', () => {
    const PIN_LENGTH = 8;
    inputField.value = '0'.repeat(PIN_LENGTH);
    expectGT(inputField.scrollWidth, 0);
    expectLE(inputField.scrollWidth, inputField.clientWidth);
  });

  // Test that the distance between characters (dots) is set in a correct way
  // and doesn't fall back to the default value.
  test('PIN input letter-spacing is correctly set up', () => {
    expectNotEquals(
        getComputedStyle(inputField).getPropertyValue('letter-spacing'),
        'normal');
  });

  // Test that the focus on the input field isn't lost when the PIN is requested
  // again after the failed verification.
  test('focus restores after progress animation', () => {
    // The PIN keyboard is displayed initially.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);

    // The PIN keyboard gets focused.
    securityTokenPin.focus();
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);

    // The user submits some value while keeping the focus on the input field.
    pinInput.value = '123';
    const enterEvent = new Event('keydown');
    enterEvent.keyCode = 13;
    pinInput.dispatchEvent(enterEvent);
    // The PIN keyboard is replaced by the animation UI.
    expectTrue(pinKeyboardContainer.hidden);
    expectFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);
    // The focus is on the input field.
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);
  });

  // Test that the input field gets focused when the PIN is requested again
  // after the failed verification.
  test('focus set after progress animation', () => {
    // The PIN keyboard is displayed initially.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);

    // The user submits some value using the "Submit" UI button.
    pinInput.value = '123';
    submitElement.click();
    // The PIN keyboard is replaced by the animation UI.
    expectTrue(pinKeyboardContainer.hidden);
    expectFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: ''
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);
    // The focus is on the input field.
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);
  });

  mocha.run();
});
