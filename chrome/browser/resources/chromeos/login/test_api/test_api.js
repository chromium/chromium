// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common testing utils methods used for OOBE tast tests.
 */

class TestElementApi {
  /**
   * Returns HTMLElement with $$ support.
   * @return {HTMLElement}
   */
  element() {
    throw 'element() should be defined!';
  }

  /**
   * Returns whether the element is visible.
   * @return {boolean}
   */
  isVisible() {
    return !this.element().hidden;
  }

  /**
   * Returns whether the element is enabled.
   * @return {boolean}
   */
  isEnabled() {
    return !this.element().disabled;
  }
}

class ScreenElementApi extends TestElementApi {
  constructor(id) {
    super();
    this.id = id;
    this.nextButton = undefined;
  }

  /** @override */
  element() {
    return $(this.id);
  }

  /**
   * Click on the primary action button ("Next" usually).
   */
  clickNext() {
    assert(this.nextButton);
    this.nextButton.click();
  }

  /**
   * Returns whether the screen should be skipped.
   * @return {boolean}
   */
  shouldSkip() {
    return false;
  }
}

class PolymerElementApi extends TestElementApi {
  constructor(parent, query) {
    super();
    this.parent = parent;
    this.query = query;
  }

  /** @override */
  element() {
    assert(this.parent.element());
    return this.parent.element().$$(this.query);
  }

  /**
   * Assert element is visible/enabled and click on the element.
   */
  click() {
    assert(this.isVisible());
    assert(this.isEnabled());
    this.element().click();
  }
}

class TextFieldApi extends PolymerElementApi {
  constructor(parent, query) {
    super(parent, query);
  }

  /**
   * Assert element is visible/enabled and fill in the element with a value.
   * @param {string} value
   */
  typeInto(value) {
    assert(this.isVisible());
    assert(this.isEnabled());
    this.element().value = value;
    this.element().dispatchEvent(new Event('input'));
    this.element().dispatchEvent(new Event('change'));
  }
}

class WelcomeScreen extends ScreenElementApi {
  constructor() {
    super('connect');
    let mainStep = new PolymerElementApi(this, '#welcomeScreen');
    this.nextButton = new PolymerElementApi(mainStep, '#welcomeNextButton');
  }
}

class NetworkScreen extends ScreenElementApi {
  constructor() {
    super('network-selection');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }
}

class EulaScreen extends ScreenElementApi {
  constructor() {
    super('oobe-eula-md');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');
  }

  /** @override */
  shouldSkip() {
    // Eula screen should be skipped when it is non-branded build.
    return !loadTimeData.getBoolean('isBrandedBuild');
  }
}

class UserCreationScreen extends ScreenElementApi {
  constructor() {
    super('user-creation');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }
}

class GaiaScreen extends ScreenElementApi {
  constructor() {
    super('gaia-signin');
  }
}

class ConfirmSamlPasswordScreen extends ScreenElementApi {
  constructor() {
    super('saml-confirm-password');
    this.passwordInput = new TextFieldApi(this, '#passwordInput');
    this.confirmPasswordInput = new TextFieldApi(this, '#confirmPasswordInput');
    this.nextButton = new PolymerElementApi(this, '#next');
  }

  /**
   * Enter password input fields with password value and submit the form.
   * @param {string} password
   */
  enterManualPasswords(password) {
    this.passwordInput.typeInto(password);
    Polymer.RenderStatus.afterNextRender(assert(this.element()), () => {
      this.confirmPasswordInput.typeInto(password);
      Polymer.RenderStatus.afterNextRender(assert(this.element()), () => {
        this.clickNext();
      });
    });
  }
}

class OobeApiProvider {
  constructor() {
    this.screens = {
      WelcomeScreen: new WelcomeScreen(),
      NetworkScreen: new NetworkScreen(),
      EulaScreen: new EulaScreen(),
      UserCreationScreen: new UserCreationScreen(),
      GaiaScreen: new GaiaScreen(),
      ConfirmSamlPasswordScreen: new ConfirmSamlPasswordScreen(),
    };
  }
}

window.OobeAPI = new OobeApiProvider();
