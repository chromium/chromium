// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A row that represents an editable Japanese dictionary entry.
 */

import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {JapaneseDictionaryEntry} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';
import {JpPosType} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import {getTemplate} from './os_japanese_dictionary_entry_row.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

interface DropdownOption {
  value: JpPosType;
  label: string;
}

interface OsJapaneseDictionaryEntryRowElement {
  $: {
    posDropdownMenu: HTMLSelectElement,
  };
}

export type EntryDeletedCustomEvent = CustomEvent<{isLastEntry: boolean}>;

class OsJapaneseDictionaryEntryRowElement extends I18nMixin
(PolymerElement) {
  // LINT.IfChange(JpPosType)
  private readonly posTypeOptions_: DropdownOption[] = [
    {value: JpPosType.kNoPos, label: '品詞なし'},
    {value: JpPosType.kNoun, label: '名詞'},
    {value: JpPosType.kAbbreviation, label: '短縮よみ'},
    {value: JpPosType.kSuggestionOnly, label: 'サジェストのみ'},
    {value: JpPosType.kProperNoun, label: '固有名詞'},
    {value: JpPosType.kPersonalName, label: '人名'},
    {value: JpPosType.kFamilyName, label: '姓'},
    {value: JpPosType.kFirstName, label: '名'},
    {value: JpPosType.kOrganizationName, label: '組織'},
    {value: JpPosType.kPlaceName, label: '地名'},
    {value: JpPosType.kSaIrregularConjugationNoun, label: '名詞サ変'},
    {value: JpPosType.kAdjectiveVerbalNoun, label: '名詞形動'},
    {value: JpPosType.kNumber, label: '数'},
    {value: JpPosType.kAlphabet, label: 'アルファベット'},
    {value: JpPosType.kSymbol, label: '記号'},
    {value: JpPosType.kEmoticon, label: '顔文字'},
    {value: JpPosType.kAdverb, label: '副詞'},
    {value: JpPosType.kPrenounAdjectival, label: '連体詞'},
    {value: JpPosType.kConjunction, label: '接続詞'},
    {value: JpPosType.kInterjection, label: '感動詞'},
    {value: JpPosType.kPrefix, label: '接頭語'},
    {value: JpPosType.kCounterSuffix, label: '助数詞'},
    {value: JpPosType.kGenericSuffix, label: '接尾一般'},
    {value: JpPosType.kPersonNameSuffix, label: '接尾人名'},
    {value: JpPosType.kPlaceNameSuffix, label: '接尾地名'},
    {value: JpPosType.kWaGroup1Verb, label: '動詞ワ行五段'},
    {value: JpPosType.kKaGroup1Verb, label: '動詞カ行五段'},
    {value: JpPosType.kSaGroup1Verb, label: '動詞サ行五段'},
    {value: JpPosType.kTaGroup1Verb, label: '動詞タ行五段'},
    {value: JpPosType.kNaGroup1Verb, label: '動詞ナ行五段'},
    {value: JpPosType.kMaGroup1Verb, label: '動詞マ行五段'},
    {value: JpPosType.kRaGroup1Verb, label: '動詞ラ行五段'},
    {value: JpPosType.kGaGroup1Verb, label: '動詞ガ行五段'},
    {value: JpPosType.kBaGroup1Verb, label: '動詞バ行五段'},
    {value: JpPosType.kHaGroup1Verb, label: '動詞ハ行四段'},
    {value: JpPosType.kGroup2Verb, label: '動詞一段'},
    {value: JpPosType.kKuruGroup3Verb, label: '動詞カ変'},
    {value: JpPosType.kSuruGroup3Verb, label: '動詞サ変'},
    {value: JpPosType.kZuruGroup3Verb, label: '動詞ザ変'},
    {value: JpPosType.kRuGroup3Verb, label: '動詞ラ変'},
    {value: JpPosType.kAdjective, label: '形容詞'},
    {value: JpPosType.kSentenceEndingParticle, label: '終助詞'},
    {value: JpPosType.kPunctuation, label: '句読点'},
    {value: JpPosType.kFreeStandingWord, label: '独立語'},
    {value: JpPosType.kSuppressionWord, label: '抑制単語'},
  ];
  // LINT.ThenChange(//chromeos/ash/services/ime/public/mojom/user_data_japanese_dictionary.mojom:JpPosType)

  static get is() {
    return 'os-japanese-dictionary-entry-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(b/340393256): Handle deserialization.
      dictId: {
        type: Number,
      },
      index: {
        type: Number,
      },
      entry: {
        type: Object,
      },
      locallyAdded: {
        type: Boolean,
      },
      isLastEntry: {
        type: Boolean,
      },
    };
  }

  // Whether the entry needs to be added to the storage.
  locallyAdded = false;

  // Whether this entry is the last entry in the dictionary.
  isLastEntry = false;

  // The ID of the Japanese User Dictionary that the entry is part of.
  dictId: bigint;

  // Index of the entry within the dictionary.
  index: number;

  // The JapaneseDictionary entry that represents a key value pair and
  // attributes.
  entry: JapaneseDictionaryEntry;

  private saveReading_(e: Event): void {
    this.entry.key = (e.target as CrInputElement).value;
    this.saveEntryToDictionary_();
  }

  private saveWord_(e: Event): void {
    this.entry.value = (e.target as CrInputElement).value;
    this.saveEntryToDictionary_();
  }

  private saveComment_(e: Event): void {
    this.entry.comment = (e.target as CrInputElement).value;
    this.saveEntryToDictionary_();
  }

  private onOptionChanged_(): void {
    const selected = this.$.posDropdownMenu.value;

    this.entry.pos = Number(selected);
    this.saveEntryToDictionary_();
  }

  private isSelectedOption_(value: JpPosType): boolean {
    return this.entry.pos === value;
  }

  private async deleteEntry_(): Promise<void> {
    if (this.locallyAdded) {
      this.dispatchEntryDeletedEvent_();
      // Clear this local entry by just syncing to mozc dictionary.
      // This will cause a UI refresh.
      this.dispatchSavedEvent_();
      return;
    }

    const dictionarySaved =
        (await UserDataServiceProvider.getRemote()
             .deleteJapaneseDictionaryEntry(this.dictId, this.index))
            .status.success;
    if (dictionarySaved) {
      this.dispatchEntryDeletedEvent_();
      this.dispatchSavedEvent_();
    }
  }

  private async saveEntryToDictionary_(): Promise<void> {
    if (this.entry.key === '' || this.entry.value === '') {
      return;
    }

    let dictionarySaved = false;
    if (this.locallyAdded) {
      // Entry does not exist inside the storage, hence we need to use the "add"
      // function to add this entry.
      // TODO(b/340393256): Handle possible race condition when two add..Entry
      // requests are in flight at the same time.
      const resp =
          (await UserDataServiceProvider.getRemote().addJapaneseDictionaryEntry(
               this.dictId, this.entry))
              .status;

      if (resp.success) {
        // If successful, then the entry is no longer "locally added", since it
        // also exists inside the storage. Future edits need to be done via the
        // "edit" api call.
        this.locallyAdded = false;
        dictionarySaved = true;
      }
    } else {
      dictionarySaved = (await UserDataServiceProvider.getRemote()
                             .editJapaneseDictionaryEntry(
                                 this.dictId, this.index, this.entry))
                            .status.success;
    }

    if (dictionarySaved) {
      this.dispatchSavedEvent_();
    }
  }

  private dispatchSavedEvent_(): void {
    this.dispatchEvent(
        new CustomEvent('dictionary-saved', {bubbles: true, composed: true}));
  }

  private dispatchEntryDeletedEvent_(): void {
    this.dispatchEvent(new CustomEvent('dictionary-entry-deleted', {
      bubbles: true,
      composed: true,
      detail: {isLastEntry: this.isLastEntry},
    }));
  }


  private i18nEntryDescription_(): string {
    // +1 to the index so that it starts at "1" instead of 0.
    return this.i18n('japaneseDictionaryEntryPosition', this.index + 1);
  }
}

customElements.define(
    OsJapaneseDictionaryEntryRowElement.is,
    OsJapaneseDictionaryEntryRowElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsJapaneseDictionaryEntryRowElement.is]:
        OsJapaneseDictionaryEntryRowElement;
  }
}

declare global {
  interface HTMLElementEventMap {
    ['dictionary-saved']: CustomEvent;
    ['dictionary-entry-deleted']: EntryDeletedCustomEvent;
  }
}
