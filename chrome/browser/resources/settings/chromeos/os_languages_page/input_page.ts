// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-input-page' is the input sub-page
 * for language and input method settings.
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './add_input_methods_dialog.js';
import './add_spellcheck_languages_dialog.js';
import './os_edit_dictionary_page.js';
import '../keyboard_shortcut_banner/keyboard_shortcut_banner.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {FocusConfig} from '../focus_config.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {hasOptionsPageInSettings} from './input_method_util.js';
import {getTemplate} from './input_page.html.js';
import {InputsShortcutReminderState, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_metrics_proxy.js';
import {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';

const OsSettingsInputPageElementBase =
    RouteObserverMixin(PrefsMixin(I18nMixin(DeepLinkingMixin(PolymerElement))));

interface OsSettingsInputPageElement {
  $: {
    addInputMethod: CrButtonElement,
    editDictionarySubpageTrigger: CrLinkRowElement,
  };
}

class OsSettingsInputPageElement extends OsSettingsInputPageElementBase {
  static get is() {
    return 'os-settings-input-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(b/265554350): Remove this property from properties() as it is
      // already specified in PrefsMixin.
      /* Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      /**
       * Read-only reference to the languages model provided by the
       * 'os-settings-languages' instance.
       */
      languages: {
        type: Object,
      },

      languageHelper: Object,

      spellCheckLanguages_: {
        type: Array,
        computed: `getSpellCheckLanguages_(languageSettingsV2Update2Enabled_,
            languages.spellCheckOnLanguages.*, languages.enabled.*)`,
      },

      showAddInputMethodsDialog_: {
        type: Boolean,
        value: false,
      },

      showAddSpellcheckLanguagesDialog_: {
        type: Boolean,
        value: false,
      },

      // TODO(b/265554350): Remove this property from properties() as it is
      // already specified in DeepLinkingMixin, and move the default value to
      // the field initializer.
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kShowInputOptionsInShelf,
          Setting.kAddInputMethod,
          Setting.kSpellCheck,
        ]),
      },

      languageSettingsV2Update2Enabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
        },
      },

      languageSettingsJapaneseEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('systemJapanesePhysicalTyping');
        },
      },

      shouldShowLanguagePacksNotice_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('languagePacksHandwritingEnabled');
        },
      },

      /**
       * Whether the shortcut reminder for the last used IME is currently
       * showing.
       */
      showLastUsedImeShortcutReminder_: {
        type: Boolean,
        computed: `shouldShowLastUsedImeShortcutReminder_(
            languages.inputMethods.enabled.length,
            prefs.ash.shortcut_reminders.last_used_ime_dismissed.value)`,
      },

      /**
       * Whether the shortcut reminder for the next IME is currently showing.
       */
      showNextImeShortcutReminder_: {
        type: Boolean,
        computed: `shouldShowNextImeShortcutReminder_(
            languages.inputMethods.enabled.length,
            prefs.ash.shortcut_reminders.next_ime_dismissed.value)`,
      },

      /**
       * The body of the currently showing shortcut reminders.
       */
      shortcutReminderBody_: {
        type: Array,
        computed: `getShortcutReminderBody_(showLastUsedImeShortcutReminder_,
            showNextImeShortcutReminder_)`,
      },

      onDeviceGrammarCheckEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('onDeviceGrammarCheckEnabled');
        },
      },
    };
  }

  // Public API: Bidirectional data flow.
  // override prefs: any;  // From PrefsMixin.

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper;
  // Note that even though this passed in using downwards data flow, we mutate
  // this variable in focusConfigChanged_. This is OK, as the place where we use
  // focusConfig (<os-settings-animated-pages>) lazily reads focusConfig and
  // does not need to be notified of mutations.
  focusConfig: FocusConfig;

  // API proxies.
  private languagesMetricsProxy_ = LanguagesMetricsProxyImpl.getInstance();

  // Internal properties for mixins.
  // From DeepLinkingMixin.
  // override supportedSettingIds = new Set<Setting>([
  //   Setting.kShowInputOptionsInShelf,
  //   Setting.kAddInputMethod,
  //   Setting.kSpellCheck,
  // ]);

  // Internal state.
  private showAddSpellcheckLanguagesDialog_: boolean;
  private showAddInputMethodsDialog_: boolean;

  // loadTimeData flags.
  private onDeviceGrammarCheckEnabled_: boolean;
  private languageSettingsV2Update2Enabled_: boolean;
  private languageSettingsJapaneseEnabled_: boolean;
  private shouldShowLanguagePacksNotice_: boolean;

  // Computed properties.
  private spellCheckLanguages_?: Array<LanguageState|SpellCheckLanguageState>;
  private showLastUsedImeShortcutReminder_: boolean;
  private showNextImeShortcutReminder_: boolean;
  // This is passed into a <keyboard-shortcut-banner> as a `body`, but that
  // takes in a `string[]`, not `TrustedHTML[]`.
  // TODO(b/238031866): Update <keyboard-shortcut-banner> to take in
  // `TrustedHTML[]`, or update this and `getShortcutReminderBody` to be a a
  // `string[]`.
  private shortcutReminderBody_: TrustedHTML[];

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.OS_LANGUAGES_INPUT) {
      return;
    }

    this.attemptDeepLink();
  }

  private focusConfigChanged_(
      _newConfig: FocusConfig, oldConfig: FocusConfig|null): void {
    // Safety: focusConfig is set only once on the parent, so this observer
    // should only fire once.
    assert(!oldConfig);
    this.focusConfig.set(
        routes.OS_LANGUAGES_EDIT_DICTIONARY.path,
        () => focusWithoutInk(this.$.editDictionarySubpageTrigger));
  }

  private onShowImeMenuChange_(e: Event): void {
    this.languagesMetricsProxy_.recordToggleShowInputOptionsOnShelf(
        // Safety: This method is only called from a
        // <settings-toggle-button> event handler.
        (e.target as SettingsToggleButtonElement).checked);
  }

  private inputMethodsLimitedByPolicy_(): boolean {
    const allowedInputMethodsPref =
        this.getPref('settings.language.allowed_input_methods');
    return !!allowedInputMethodsPref && allowedInputMethodsPref.value.length;
  }

  /**
   * Handler for click events on an input method on the main page,
   * which sets it as the current input method.
   */
  private onInputMethodClick_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.InputMethod>): void {
    // Clicks on the button are handled in onInputMethodOptionsClick_.
    // Safety: This event comes from the DOM, so the target should always be an
    // element.
    if ((e.target as Element).tagName === 'CR-ICON-BUTTON') {
      return;
    }

    this.languageHelper.setCurrentInputMethod(e.model.item.id);
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.SWITCH_INPUT_METHOD);
    recordSettingChange();
  }

  /**
   * Handler for <Enter> events on an input method on the main page,
   * which sets it as the current input method.
   */
  private onInputMethodKeyPress_(
      e: DomRepeatEvent<
          chrome.languageSettingsPrivate.InputMethod, KeyboardEvent>): void {
    // Ignores key presses other than <Enter>.
    if (e.key !== 'Enter') {
      return;
    }

    this.languageHelper.setCurrentInputMethod(e.model.item.id);
  }

  /**
   * Opens the input method extension's options page in a new tab (or focuses
   * an existing instance of the IME's options).
   */
  private openExtensionOptionsPage_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.InputMethod>): void {
    this.languageHelper.openInputMethodOptions(e.model.item.id);
  }

  /**
   * @param id The input method ID.
   * @return True if there is a options page in ChromeOS settings for the input
   *     method ID.
   */
  private hasOptionsPageInSettings_(id: string): boolean {
    return hasOptionsPageInSettings(
        id, loadTimeData.getBoolean('allowPredictiveWriting'),
        loadTimeData.getBoolean('systemJapanesePhysicalTyping'));
  }

  private navigateToOptionsPageInSettings_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.InputMethod>): void {
    const params = new URLSearchParams();
    params.append('id', e.model.item.id);
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS, params);
  }

  /**
   * @param id The input method ID.
   * @param currentId The ID of the currently enabled input method.
   * @return True if the IDs match.
   */
  private isCurrentInputMethod_(id: string, currentId: string): boolean {
    return id === currentId;
  }

  /**
   * @param id The input method ID.
   * @param currentId The ID of the currently enabled input method.
   * @return The class for the input method item.
   */
  private getInputMethodItemClass_(id: string, currentId: string): string {
    return this.isCurrentInputMethod_(id, currentId) ? 'selected' : '';
  }

  /**
   * @param id The selected input method ID.
   * @param currentId The ID of the currently enabled input method.
   * @return The default tab index '0' if the selected input method is not
   *     currently enabled; otherwise, returns an empty string which effectively
   *     unsets the tabindex attribute.
   */
  private getInputMethodTabIndex_(id: string, currentId: string): string {
    return id === currentId ? '' : '0';
  }

  private getOpenOptionsPageLabel_(inputMethodName: string): string {
    return this.i18n('openOptionsPage', inputMethodName);
  }

  private onAddInputMethodClick_(): void {
    this.languagesMetricsProxy_.recordAddInputMethod();
    this.showAddInputMethodsDialog_ = true;
  }

  private onAddInputMethodsDialogClose_(): void {
    this.showAddInputMethodsDialog_ = false;
    focusWithoutInk(this.$.addInputMethod);
  }

  private onAddSpellcheckLanguagesClick_(): void {
    this.showAddSpellcheckLanguagesDialog_ = true;
  }

  private onAddSpellcheckLanguagesDialogClose_(): void {
    this.showAddSpellcheckLanguagesDialog_ = false;

    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    if (this.languages!.spellCheckOnLanguages.length > 0) {
      // User has at least one spell check language after closing the dialog.
      // If spell checking is disabled, enabled it.
      this.setPrefValue('browser.enable_spellchecking', true);
    }

    // Because #addSpellcheckLanguages is not statically created (as it is
    // within a <template is="dom-if">), we need to use
    // this.shadowRoot.querySelector("#addSpellcheckLanguages") instead of
    // this.$.addSpellCheckLanguages.
    // TODO(b/263823772): Move addSpellcheckLanguages to `this.$` once we remove
    // LSV2 Update 2 code.
    focusWithoutInk(
        // Safety: This method is only called when the spell check
        // language dialog is closed, but that can only be opened if
        // #addSpellchecklanguages was clicked.
        castExists(this.shadowRoot!.querySelector('#addSpellcheckLanguages')));
  }

  private disableRemoveInputMethod_(
      targetInputMethod: chrome.languageSettingsPrivate.InputMethod): boolean {
    // Third-party IMEs can always be removed.
    if (!this.languageHelper.isComponentIme(targetInputMethod)) {
      return false;
    }

    // Disable remove if there is no other component IME (pre-installed
    // system IMES) enabled.
    // Safety: This method is only called from a button inside a `dom-repeat`
    // over `languages.inputMethods.enabled`, so if this method is called,
    // `this.languages` must be defined.
    return !this.languages!
                // Safety: `LanguagesModel.inputMethods` is always defined on
                // CrOS.
                .inputMethods!.enabled.some(
                    inputMethod => inputMethod.id !== targetInputMethod.id &&
                        this.languageHelper.isComponentIme(inputMethod));
  }

  private getRemoveInputMethodTooltip_(
      inputMethod: chrome.languageSettingsPrivate.InputMethod): string {
    return this.i18n('removeInputMethodTooltip', inputMethod.displayName);
  }

  private onRemoveInputMethodClick_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.InputMethod>): void {
    this.languageHelper.removeInputMethod(e.model.item.id);
    recordSettingChange();
  }

  private getRemoveSpellcheckLanguageTooltip_(lang: SpellCheckLanguageState):
      string {
    return this.i18n(
        'removeSpellCheckLanguageTooltip', lang.language.displayName);
  }

  private onRemoveSpellcheckLanguageClick_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>): void {
    this.languageHelper.toggleSpellCheck(e.model.item.language.code, false);
    recordSettingChange();
  }

  /**
   * Called whenever the spell check toggle is changed by the user.
   */
  private onSpellcheckToggleChange_(e: Event): void {
    // Safety: This is only called from a 'settings-boolean-control-changed'
    // event from a <settings-toggle-button>, so the event target must be a
    // <settings-toggle-button>.
    const toggle = (e.target as SettingsToggleButtonElement);

    this.languagesMetricsProxy_.recordToggleSpellCheck(toggle.checked);

    if (this.languageSettingsV2Update2Enabled_ && toggle.checked &&
        // This assertion of `this.languages` is potentially unsafe and could
        // fail.
        // TODO(b/265553377): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        this.languages!.spellCheckOnLanguages.length === 0) {
      // In LSV2 Update 2, we never want to enable spell check without the user
      // having a spell check language. When this happens, we try estimating
      // their expected spell check language (their device language, assuming
      // that the user has an input method which supports that language).
      // If that doesn't work, we fall back on prompting the user to enable a
      // spell check language and immediately disable spell check before this
      // happens. If the user then adds a spell check language, we finally
      // enable spell check (see |onAddSpellcheckLanguagesDialogClose_|).

      // Safety: `LanguagesModel.prospectiveUILanguage` is always defined on
      // CrOS, and we checked that `this.languages` is defined above.
      const deviceLanguageCode =
          castExists(this.languages!.prospectiveUILanguage);
      // However, deviceLanguage itself may be undefined as it is possible that
      // it was set outside of CrOS language settings (normally when debugging
      // or in tests).
      const deviceLanguage =
          this.languageHelper.getLanguage(deviceLanguageCode);
      if (deviceLanguage && deviceLanguage.supportsSpellcheck &&
          // Safety: `LanguagesModel.inputMethods` is always defined on CrOS,
          // and we checked that `this.languages` is defined above.
          this.languages!.inputMethods!.enabled.some(
              inputMethod =>
                  inputMethod.languageCodes.includes(deviceLanguageCode))) {
        this.languageHelper.toggleSpellCheck(deviceLanguageCode, true);
      } else {
        this.onAddSpellcheckLanguagesClick_();

        // "Undo" the toggle change by reverting it back to the original pref
        // value. The toggle will be flipped on once the user finishes adding
        // a spell check language.
        toggle.resetToPrefValue();
        // We don't need to commit the pref change below, so early return.
        return;
      }
    }

    // Manually commit the pref change as we've set noSetPref on the toggle
    // button.
    toggle.sendPrefChange();
  }

  /**
   * Returns the value to use as the |pref| attribute for the policy indicator
   * of spellcheck languages, based on whether or not the language is enabled.
   * @param isEnabled Whether the language is enabled or not.
   */
  private getIndicatorPrefForManagedSpellcheckLanguage_(isEnabled: boolean):
      chrome.settingsPrivate.PrefObject<string[]> {
    return isEnabled ?
        this.getPref<string[]>('spellcheck.forced_dictionaries') :
        this.getPref<string[]>('spellcheck.blocked_dictionaries');
  }

  /**
   * Returns an array of spell check languages for the UI.
   */
  private getSpellCheckLanguages_():
      Array<LanguageState|SpellCheckLanguageState>|undefined {
    if (this.languages === undefined) {
      return undefined;
    }
    if (this.languageSettingsV2Update2Enabled_) {
      const languages = [...this.languages.spellCheckOnLanguages];
      languages.sort(
          (a, b) =>
              a.language.displayName.localeCompare(b.language.displayName));
      return languages;
    }
    const enabledLanguages: Array<LanguageState|SpellCheckLanguageState> =
        this.languages.enabled;
    const combinedLanguages =
        enabledLanguages.concat(this.languages.spellCheckOnLanguages);
    const supportedSpellcheckLanguagesSet = new Set<string>();
    const supportedSpellcheckLanguages:
        Array<LanguageState|SpellCheckLanguageState> = [];

    combinedLanguages.forEach(languageState => {
      if (!supportedSpellcheckLanguagesSet.has(languageState.language.code) &&
          languageState.language.supportsSpellcheck) {
        supportedSpellcheckLanguages.push(languageState);
        supportedSpellcheckLanguagesSet.add(languageState.language.code);
      }
    });

    return supportedSpellcheckLanguages;
  }

  /**
   * Handler for enabling or disabling spell check for a specific language.
   */
  private onSpellCheckLanguageChange_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>): void {
    const item = e.model.item;
    if (!item.language.supportsSpellcheck) {
      return;
    }

    this.languageHelper.toggleSpellCheck(
        item.language.code, !item.spellCheckEnabled);
  }

  /**
   * Handler for clicking on the name of the language. The action taken must
   * match the control that is available.
   */
  private onSpellCheckNameClick_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>): void {
    // Safety: The button associated with this event listener is disabled in
    // the template if the below is true.
    assert(!this.isSpellCheckNameClickDisabled_(e.model.item));
    this.onSpellCheckLanguageChange_(e);
  }

  /**
   * Name only supports clicking when language is not managed, supports
   * spellcheck, and the dictionary has been downloaded with no errors.
   */
  private isSpellCheckNameClickDisabled_(item: LanguageState|
                                         SpellCheckLanguageState): boolean {
    return item.isManaged || item.downloadDictionaryFailureCount > 0 ||
        !this.getPref('browser.enable_spellchecking').value;
  }

  /**
   * Handler to initiate another attempt at downloading the spell check
   * dictionary for a specified language.
   */
  private onRetryDictionaryDownloadClick_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>): void {
    // Safety: The button associated with this event listener is disabled in
    // the template if `!item.downloadDictionaryFailureCount`, and dictionary
    // download failure count cannot ever be negative.
    assert(e.model.item.downloadDictionaryFailureCount > 0);
    this.languageHelper.retryDownloadDictionary(e.model.item.language.code);
  }

  private getDictionaryDownloadRetryAriaLabel_(item: LanguageState): string {
    return this.i18n(
        'languagesDictionaryDownloadRetryDescription',
        item.language.displayName);
  }

  private onEditDictionaryClick_(): void {
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_CUSTOM_SPELL_CHECK);
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_EDIT_DICTIONARY);
  }

  private onJapaneseManageUserDictionaryClick_(): void {
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY);
  }

  /**
   * Gets the appropriate CSS class for the Enhanced spell check toggle
   * depending on whether Update 2 is enabled or not.
   */
  private getEnhancedSpellCheckClass_(): ''|'hr' {
    return this.languageSettingsV2Update2Enabled_ ? '' : 'hr';
  }

  private isEnableSpellcheckingDisabled_(): boolean {
    return !this.languageSettingsV2Update2Enabled_ &&
        (!!this.spellCheckLanguages_ && this.spellCheckLanguages_.length === 0);
  }

  private isCollapseOpened_(update2Enabled: boolean, spellCheckOn: boolean):
      boolean {
    return !update2Enabled || spellCheckOn;
  }

  private onLanguagePackNoticeLinkClick_(): void {
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_LANGUAGE_PACKS_LEARN_MORE);
  }

  private shouldShowLastUsedImeShortcutReminder_(): boolean {
    // User has already dismissed the shortcut reminder.
    if (this.getPref('ash.shortcut_reminders.last_used_ime_dismissed').value) {
      return false;
    }
    // Need at least 2 input methods to be shown the reminder.
    // Safety: `LanguagesModel.inputMethods` is always defined on CrOS.
    return !!this.languages && this.languages.inputMethods!.enabled.length >= 2;
  }

  private shouldShowNextImeShortcutReminder_(): boolean {
    // User has already dismissed the shortcut reminder.
    if (this.getPref('ash.shortcut_reminders.next_ime_dismissed').value) {
      return false;
    }
    // Need at least 3 input methods to be shown the reminder.
    // Safety: `LanguagesModel.inputMethods` is always defined on CrOS.
    return !!this.languages && this.languages.inputMethods!.enabled.length >= 3;
  }

  private getShortcutReminderBody_(): TrustedHTML[] {
    const reminderBody: TrustedHTML[] = [];
    if (this.showLastUsedImeShortcutReminder_) {
      reminderBody.push(this.i18nAdvanced('imeShortcutReminderLastUsed'));
    }
    if (this.showNextImeShortcutReminder_) {
      reminderBody.push(this.i18nAdvanced('imeShortcutReminderNext'));
    }
    return reminderBody;
  }

  private shouldShowShortcutReminder_(): boolean {
    return this.languageSettingsV2Update2Enabled_ &&
        // `this.shortcutReminderBody_` should always be truthy here.
        // TODO(b/238031866): Remove `this.shortcutReminderBody_` here, or
        // investigate why removing it does not work.
        this.shortcutReminderBody_ && this.shortcutReminderBody_.length > 0;
  }

  private onShortcutReminderDismiss_(): void {
    // Record the metric - assume that both reminders were dismissed unless one
    // of them wasn't shown.
    // Safety: The shortcut reminder is only shown if
    // `shouldShowShortcutReminder_` is true, so `this.shortcutReminderBody`
    // contains at least one thing, so at least one of
    // `this.showLastUsedImeShortcutReminder_` or
    // `this.showNextImeShortcutReminder_` should be true.
    assert(
        this.showLastUsedImeShortcutReminder_ ||
        this.showNextImeShortcutReminder_);
    let dismissedState = InputsShortcutReminderState.LAST_USED_IME_AND_NEXT_IME;
    if (!this.showLastUsedImeShortcutReminder_) {
      dismissedState = InputsShortcutReminderState.NEXT_IME;
    } else if (!this.showNextImeShortcutReminder_) {
      dismissedState = InputsShortcutReminderState.LAST_USED_IME;
    }
    this.languagesMetricsProxy_.recordShortcutReminderDismissed(dismissedState);

    if (this.showLastUsedImeShortcutReminder_) {
      this.setPrefValue('ash.shortcut_reminders.last_used_ime_dismissed', true);
    }
    if (this.showNextImeShortcutReminder_) {
      this.setPrefValue('ash.shortcut_reminders.next_ime_dismissed', true);
    }
  }
}

customElements.define(
    OsSettingsInputPageElement.is, OsSettingsInputPageElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsInputPageElement.is]: OsSettingsInputPageElement;
  }
}
