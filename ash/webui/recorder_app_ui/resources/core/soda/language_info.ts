// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Language code used for transcription.
 *
 * This enum is sorted in the same order as the `LanguageCode` in
 * components/soda/constants.h.
 */
export enum LanguageCode {
  EN_US = 'en-US',
  JA_JP = 'ja-JP',
  DE_DE = 'de-DE',
  ES_ES = 'es-ES',
  FR_FR = 'fr-FR',
  IT_IT = 'it-IT',
  EN_CA = 'en-CA',
  EN_AU = 'en-AU',
  EN_GB = 'en-GB',
  EN_IE = 'en-IE',
  EN_SG = 'en-SG',
  FR_BE = 'fr-BE',
  FR_CH = 'fr-CH',
  EN_IN = 'en-IN',
  IT_CH = 'it-CH',
  DE_AT = 'de-AT',
  DE_BE = 'de-BE',
  DE_CH = 'de-CH',
  ES_US = 'es-US',
  HI_IN = 'hi-IN',
  PT_BR = 'pt-BR',
  ID_ID = 'id-ID',
  KO_KR = 'ko-KR',
  PL_PL = 'pl-PL',
  TH_TH = 'th-TH',
  TR_TR = 'tr-TR',
  ZH_CN = 'cmn-Hans-CN',
  ZH_TW = 'cmn-Hant-TW',
  DA_DK = 'da-DK',
  FR_CA = 'fr-CA',
  NB_NO = 'nb-NO',
  NL_NL = 'nl-NL',
  SV_SE = 'sv-SE',
  RU_RU = 'ru-RU',
  VI_VN = 'vi-VN',
}

export interface LangPackInfo {
  languageCode: LanguageCode;

  /**
   * Language name displayed in the application locale.
   */
  displayName: string;

  /**
   * Whether summarization and title suggestion support this language.
   */
  isGenAiSupported: boolean;

  /**
   * Whether speaker label supports this language.
   */
  isSpeakerLabelSupported: boolean;
}
