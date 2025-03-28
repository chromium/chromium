// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Defines methods related to retrieving translated messages.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

export class Msgs {
  /** Return the current locale. */
  static getLocale(): string {
    return chrome.i18n.getMessage('locale');
  }

  /**
   * Returns the message with the given message id from the ChromeVox namespace.
   *
   * If we can't find a message, throw an exception.  This allows us to catch
   * typos early.
   *
   * @param messageId The id.
   * @param subs Substitution strings.
   * @return The localized message.
   */
  static getMsg(messageId: string, subs?: string[]): string {
    let message: string = Msgs.Untranslated[messageId.toUpperCase()];
    if (message !== undefined) {
      return Msgs.applySubstitutions_(message, subs);
    }
    message = chrome.i18n.getMessage(Msgs.NAMESPACE_ + messageId, subs);
    if ((message === undefined || message === '') &&
        messageId.endsWith('_brl')) {
      // Braille string entries are optional. If we couldn't find a braille-
      // specific string, try again without the '_brl' suffix.
      message = chrome.i18n.getMessage(
          Msgs.NAMESPACE_ + messageId.replace('_brl', ''), subs);
    }

    if (message === undefined || message === '') {
      throw new Error('Invalid ChromeVox message id: ' + messageId);
    }
    return message;
  }

  /**
   * Returns the message with the given message ID, formatted for the given
   * count.
   * @param subs Substitution strings.
   * @return The localized and formatted message.
   */
  static getMsgWithCount(
    messageId: string, count: number, subs?: string[]): string {
    return new goog.i18n.MessageFormat(Msgs.getMsg(messageId, subs))
        .format({COUNT: count});
  }

  /**
   * Processes an HTML DOM, replacing text content with translated text messages
   * on elements marked up for translation.  Elements whose class attributes
   * contain the 'i18n' class name are expected to also have an msgid
   * attribute. The value of the msgid attributes are looked up as message
   * IDs and the resulting text is used as the text content of the elements.
   *
   * @param root The root node where the translation should be performed.
   */
  static addTranslatedMessagesToDom(root: Element|Document): void {
    const elts = root.querySelectorAll('.i18n');
    for (let i = 0; i < elts.length; i++) {
      const msgid = elts[i].getAttribute('msgid');
      if (!msgid) {
        throw new Error('Element has no msgid attribute: ' + elts[i]);
      }
      const val = Msgs.getMsg(msgid);
      if (elts[i].tagName === 'INPUT') {
        elts[i].setAttribute('placeholder', val);
      } else {
        elts[i].textContent = val;
      }
      elts[i].classList.add('i18n-processed');
    }
  }

  /**
   * Returns a number formatted correctly.
   * @return The number in the correct locale.
   */
  static getNumber(num: number): string {
    return '' + num;
  }

  /**
   * Applies substitutions of the form $N, where N is a number from 1 to 9, to a
   * string. The numbers are one-based indices into |opt_subs|.
   */
  private static applySubstitutions_(message: string, subs?: string[]): string {
    if (subs) {
      for (let i = 0; i < subs.length; i++) {
        message = message.replace('$' + (i + 1), subs[i]);
      }
    }
    return message;
  }
}

export namespace Msgs {
/** The namespace for all Chromevox messages. */
export const NAMESPACE_ = 'chromevox_';

/**
 * Strings that are displayed in the user interface but don't need
 * be translated.
 */
export const Untranslated: Record<string, string> = {
  /** The unchecked state for a checkbox in braille. */
  CHECKBOX_UNCHECKED_STATE_BRL: '( )',
  /** The checked state for a checkbox in braille. */
  CHECKBOX_CHECKED_STATE_BRL: '(x)',
  /** The unselected state for a radio button in braille. */
  RADIO_UNSELECTED_STATE_BRL: '( )',
  /** The selected state for a radio button in braille. */
  RADIO_SELECTED_STATE_BRL: '(x)',
  /** Brailled after a menu if the menu has a submenu. */
  ARIA_HAS_SUBMENU_BRL: '->',
  /** Describes an element with the ARIA role option. */
  ROLE_OPTION: ' ',
  /** Braille of element with the ARIA role option. */
  ROLE_OPTION_BRL: ' ',
  /** Braille of element that is checked. */
  CHECKED_TRUE_BRL: '(x)',
  /** Braille of element that is unchecked. */
  CHECKED_FALSE_BRL: '( )',
  /** Braille of element where the checked state is mixed or indeterminate. */
  CHECKED_MIXED_BRL: '(-)',
  /** Braille of element with the ARIA attribute aria-disabled=true. */
  ARIA_DISABLED_TRUE_BRL: 'xx',
  /** Braille of element with the ARIA attribute aria-expanded=true. */
  ARIA_EXPANDED_TRUE_BRL: '-',
  /** Braille of element with the ARIA attribute aria-expanded=false. */
  ARIA_EXPANDED_FALSE_BRL: '+',
  /** Braille of element with the ARIA attribute aria-invalid=true. */
  ARIA_INVALID_TRUE_BRL: '!',
  /** Braille of element with the ARIA attribute aria-pressed=true. */
  ARIA_PRESSED_TRUE_BRL: '=',
  /** Braille of element with the ARIA attribute aria-pressed=false. */
  ARIA_PRESSED_FALSE_BRL: ' ',
  /** Braille of element with the ARIA attribute aria-pressed=mixed. */
  ARIA_PRESSED_MIXED_BRL: '-',
  /** Braille of element with the ARIA attribute aria-selected=true. */
  ARIA_SELECTED_TRUE_BRL: '(x)',
  /** Braille of element with the ARIA attribute aria-selected=false. */
  ARIA_SELECTED_FALSE_BRL: '( )',
  /** Brailled after a menu if it has a submenu. */
  HAS_SUBMENU_BRL: '->',
  /** Brailled to describe a <time> tag. */
  TAG_TIME_BRL: ' ',
  /** Spoken when describing an ARIA value. */
  ARIA_VALUE_NOW: '$1',
  /** Brailled when describing an ARIA value. */
  ARIA_VALUE_NOW_BRL: '$1',
  /** Spoken when describing an ARIA value text. */
  ARIA_VALUE_TEXT: '$1',
  /** Brailled when describing an ARIA value text. */
  ARIA_VALUE_TEXT_BRL: '$1',
};
}

TestImportManager.exportForTesting(Msgs);
