// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Static mapping from a TTS engine's `SpeechSynthesisVoice.name`
 * to a human-friendly nature name and description.
 *
 * This table is DISPLAY-ONLY. `SpeechSynthesisVoice.name` remains the real
 * identity (preferences, filtering, default-voice selection, UMA bucketing,
 * radio button values, test ids).
 *
 * This file is the source of truth for these strings and is maintained by
 * hand. Edit entries here.
 *
 * Keys are matched exactly. A key without a 'Google' identifier is a ChromeOS
 * voice: getVoiceTitle() collapses such voices to a generic system label on
 * other platforms, so those mappings only ever apply on ChromeOS.
 *
 * Nature names and descriptions are intentionally hardcoded instead of
 * localized to make sure that the strings are displayed in the same language as
 * the voice language rather than the Chrome language (i.e., Portuguese voices
 * always show in Portuguese).
 *
 * TODO(crbug.com/565099954): add lookup-miss observability so that key drift is
 * detectable to readjust key values.
 */

/**
 * Display strings for one voice.
 *
 * NOTE ON NAMING: "nature" here is unrelated to the "(Natural)" voice-quality
 * tier from methods like hasNaturalIdentifier() and getNaturalVoiceOrDefault().
 * "(Natural)" describes voice quality in SpeechSynthesisVoice.name, whereas a
 * "nature" name is a display label ('Pebble', 'Forår').
 */
export interface VoiceNatureNaming {
  // Nature word, e.g. 'Pebble'.
  natureName: string;
  // Voice descriptor. e.g. 'Gentle tone, mid pitch'.
  description: string;
}

/**
 * Grouped by locale so that related voices read together. This grouping also
 * helps with a couple tests.
 *
 * DO NOT EDIT this directly unless matching updates are approved in
 * go/reading-mode-nature-names or equivalent doc. Changes require explicit
 * approval from owners.
*/
// LINT.IfChange(VOICE_NATURE_NAMING_BY_LOCALE)
export const VOICE_NATURE_NAMING_BY_LOCALE: Readonly<
    Record<string, Readonly<Record<string, VoiceNatureNaming>>>> = {
  'bn-bd': {
    'Google বাংলা (Natural)':
        {natureName: 'ফরেস্ট', description: 'রিল্যাক্সড টোন, লো পিচ'},
  },
  'cs-cz': {
    'Google čeština (Natural)':
        {natureName: 'Zátoka', description: 'Klidný hlas, střední výška'},
  },
  'da-dk': {
    'Google Dansk 1 (Natural)':
        {natureName: 'Forår', description: 'Frisk, mellem tone'},
    'Google Dansk 2 (Natural)':
        {natureName: 'Havn', description: 'Blid, dyb tone'},
    'Google Dansk 3 (Natural)':
        {natureName: 'Middag', description: 'Lys, mellem tone'},
    'Google Dansk 4 (Natural)':
        {natureName: 'Måne', description: 'Rolig, mellem tone'},
  },
  'de-de': {
    'Google Deutsch 1 (Natural)':
        {natureName: 'Bach', description: 'Entschlossen, hellere Stimme'},
    'Google Deutsch 2 (Natural)':
        {natureName: 'Saatkorn', description: 'Sanft, hellere Stimme'},
    'Google Deutsch 3 (Natural)':
        {natureName: 'Kiesel', description: 'Aufgeweckt, dunklere Stimme'},
    'Google Deutsch 4 (Natural)':
        {natureName: 'Moos', description: 'Warm, dunklere Stimme'},
  },
  'el-gr': {
    'Google Ελληνικά (Natural)':
        {natureName: 'Ανατολή', description: 'Διαυγής, μεσαίος τόνος'},
  },
  'en-au': {
    'Google Australian English 1 (Natural)':
        {natureName: 'Mica', description: 'Bright tone, mid pitch'},
    'Google Australian English 2 (Natural)':
        {natureName: 'Strait', description: 'Breezy tone, low pitch'},
    'Google Australian English 3 (Natural)':
        {natureName: 'Bay', description: 'Calm tone, mid pitch'},
    'Google Australian English 4 (Natural)':
        {natureName: 'Gorge', description: 'Relaxed tone, low pitch'},
  },
  'en-gb': {
    'Google UK English 1 (Natural)':
        {natureName: 'Canyon', description: 'Calm tone, low pitch'},
    'Google UK English 2 (Natural)':
        {natureName: 'Grove', description: 'Gentle tone, mid pitch'},
    'Google UK English 3 (Natural)':
        {natureName: 'Ledge', description: 'Relaxed tone, low pitch'},
    'Google UK English 4 (Natural)':
        {natureName: 'Fern', description: 'Bright tone, mid pitch'},
    'Google UK English 5 (Natural)':
        {natureName: 'Forest', description: 'Soothing tone, low pitch'},
    'Google UK English 6 (Natural)':
        {natureName: 'Cove', description: 'Breezy tone, mid pitch'},
  },
  'en-us': {
    'Google US English 1 (Natural)':
        {natureName: 'Pebble', description: 'Gentle tone, mid pitch'},
    'Google US English 2 (Natural)':
        {natureName: 'Mesa', description: 'Bright tone, mid pitch'},
    'Google US English 3 (Natural)':
        {natureName: 'Slate', description: 'Peaceful tone, low pitch'},
    'Google US English 4 (Natural)':
        {natureName: 'Cedar', description: 'Relaxed tone, low pitch'},
    'Google US English 5 (Natural)':
        {natureName: 'Moss', description: 'Warm tone, mid pitch'},
    'Google US English 6 (Natural)':
        {natureName: 'Ridge', description: 'Breezy tone, low pitch'},
    'Google US English 7 (Natural)':
        {natureName: 'Sea', description: 'Serene tone, mid pitch'},
  },
  'es-es': {
    'Google español 1 (Natural)':
        {natureName: 'Ensenada', description: 'Voz serena, tono medio'},
    'Google español 2 (Natural)':
        {natureName: 'Ola', description: 'Voz resuelta, tono medio'},
    'Google español 3 (Natural)':
        {natureName: 'Pizarra', description: 'Voz resuelta, tono bajo'},
    'Google español 4 (Natural)':
        {natureName: 'Arroyo', description: 'Voz alegre, tono medio'},
    'Google español 5 (Natural)':
        {natureName: 'Marisma', description: 'Voz relajada, tono bajo'},
  },
  'es-us': {
    'Google español de Estados Unidos 1 (Natural)':
        {natureName: 'Orquídea', description: 'Voz cálida, tono medio'},
    'Google español de Estados Unidos 2 (Natural)':
        {natureName: 'Amanecer', description: 'Voz resuelta, tono medio'},
    'Google español de Estados Unidos 3 (Natural)':
        {natureName: 'Viento', description: 'Voz enérgica, tono bajo'},
    'Google español de Estados Unidos 4 (Natural)':
        {natureName: 'Mediodía', description: 'Voz cálida, tono bajo'},
  },
  'fi-fi': {
    'Google Suomi (Natural)':
        {natureName: 'Metsä', description: 'Tyyni sävy, keskiäänenkorkeus'},
  },
  'fil-ph': {
    'Google Filipino 1 (Natural)':
        {natureName: 'Mica', description: 'Relaxed na tone, mid pitch'},
    'Google Filipino 2 (Natural)':
        {natureName: 'Fern', description: 'Kalmado na tone, mid pitch'},
    'Google Filipino 3 (Natural)':
        {natureName: 'Granite', description: 'Relaxed na tone, low pitch'},
    'Google Filipino 4 (Natural)':
        {natureName: 'Harbor', description: 'Malumanay na tone, low pitch'},
  },
  'fr-fr': {
    'Google français 1 (Natural)':
        {natureName: 'Baie', description: 'Calme, ton moyen'},
    'Google français 2 (Natural)':
        {natureName: 'Crête', description: 'Vif, ton moyen'},
    'Google français 3 (Natural)':
        {natureName: 'Vague', description: 'Tranquille, ton grave'},
    'Google français 4 (Natural)':
        {natureName: 'Brise', description: 'Douce, ton moyen'},
    'Google français 5 (Natural)':
        {natureName: 'Soirée', description: 'Tranquille, ton grave'},
  },
  'hi-in': {
    'Google हिन्दी 1 (Natural)':
        {natureName: 'झील', description: 'शांत टोन, लो पिच'},
    'Google हिन्दी 2 (Natural)':
        {natureName: 'लहर', description: 'खुशनुमा टोन, मिड पिच'},
    'Google हिन्दी 3 (Natural)':
        {natureName: 'बादल', description: 'सुकूनभरी टोन, लो पिच'},
    'Google हिन्दी 4 (Natural)':
        {natureName: 'समंदर', description: 'जोशभरी टोन, मिड पिच'},
  },
  'hu-hu': {
    'Google Magyar (Natural)':
        {natureName: 'Hold', description: 'Nyugodt hang, közepes tónus'},
  },
  'id-id': {
    'Google Bahasa Indonesia 1 (Natural)':
        {natureName: 'Cerah', description: 'Tone energik, pitch medium'},
    'Google Bahasa Indonesia 2 (Natural)':
        {natureName: 'Flora', description: 'Tone tenang, pitch medium'},
    'Google Bahasa Indonesia 3 (Natural)':
        {natureName: 'Awan', description: 'Tone hangat, pitch rendah'},
    'Google Bahasa Indonesia 4 (Natural)':
        {natureName: 'Lembah', description: 'Tone tenang, pitch rendah'},
  },
  'it-it': {
    'Google italiano 1 (Natural)':
        {natureName: 'Rugiada', description: 'Vivace, tono medio'},
    'Google italiano 2 (Natural)':
        {natureName: 'Brezza', description: 'Fresca, tono medio'},
    'Google italiano 3 (Natural)':
        {natureName: 'Falesia', description: 'Gentile, tono basso'},
    'Google italiano 4 (Natural)':
        {natureName: 'Corallo', description: 'Calda, tono basso'},
  },
  'ja-jp': {
    'Google 日本語 1 (Natural)':
        {natureName: 'そよ風', description: 'さわやかな声、中音域'},
    'Google 日本語 2 (Natural)':
        {natureName: '杉', description: '温かみのある声、低音域'},
    'Google 日本語 3 (Natural)':
        {natureName: '渓谷', description: '穏やかな声、低音域'},
  },
  // ChromeOS-only: this key has no Google identifier, so this mapping is
  // unreachable on other platforms.
  'km-kh': {
    'Chrome OS ខ្មែរ':
        {natureName: 'ស៊ីលីកា', description: 'សំឡេងស្រួយ កម្រិតកណ្ដាល'},
  },
  'ko-kr': {
    'Google 한국어 1 (Natural)':
        {natureName: '봄', description: '밝은 톤, 중간 음조'},
    'Google 한국어 2 (Natural)':
        {natureName: '대리석', description: '굵은 톤, 중간 음조'},
    'Google 한국어 3 (Natural)':
        {natureName: '저녁', description: '차분한 톤, 낮은 음조'},
    'Google 한국어 4 (Natural)':
        {natureName: '바람', description: '가벼운 톤, 낮은 음조'},
  },
  'nb-no': {
    'Google Norsk Bokmål 1 (Natural)':
        {natureName: 'Bekk', description: 'Varm tone, lys stemme'},
    'Google Norsk Bokmål 2 (Natural)':
        {natureName: 'Blomst', description: 'Sindig tone, medium stemme'},
    'Google Norsk Bokmål 3 (Natural)':
        {natureName: 'Bjørk', description: 'Rolig tone, medium stemme'},
    'Google Norsk Bokmål 4 (Natural)':
        {natureName: 'Fjær', description: 'Lett tone, lys stemme'},
    'Google Norsk Bokmål 5 (Natural)':
        {natureName: 'Furu', description: 'Energisk tone, medium stemme'},
  },
  'ne-np': {
    'Google नेपाली (Natural)':
        {natureName: 'इन्लेट', description: 'स्पष्ट आवाज, मध्यम तीव्रता'},
  },
  'nl-nl': {
    'Google Nederlands 1 (Natural)':
        {natureName: 'Beek', description: 'Kalme toon, middentoonhoogte'},
    'Google Nederlands 2 (Natural)':
        {natureName: 'Bos', description: 'Rustige toon, lage toonhoogte'},
    'Google Nederlands 3 (Natural)':
        {natureName: 'Moeras', description: 'Luchtige toon, lage toonhoogte'},
    'Google Nederlands 4 (Natural)':
        {natureName: 'Bloem', description: 'Opgewekte toon, middentoonhoogte'},
    'Google Nederlands 5 (Natural)':
        {natureName: 'Maan', description: 'Luchtige toon, middentoonhoogte'},
  },
  'pl-pl': {
    'Google Polski 1 (Natural)':
        {natureName: 'Ocean', description: 'Głos pogodny, ton średni'},
    'Google Polski 2 (Natural)':
        {natureName: 'Łąka', description: 'Głos jasny, ton średni'},
    'Google Polski 3 (Natural)':
        {natureName: 'Mech', description: 'Głos łagodny, ton niski'},
    'Google Polski 4 (Natural)':
        {natureName: 'Chmura', description: 'Głos spokojny, ton niski'},
    'Google Polski 5 (Natural)':
        {natureName: 'Jezioro', description: 'Głos energiczny, ton średni'},
  },
  'pt-br': {
    'Google português do Brasil 1 (Natural)':
        {natureName: 'Vale', description: 'Voz serena, tom médio'},
    'Google português do Brasil 2 (Natural)':
        {natureName: 'Pântano', description: 'Voz amigável, tom grave'},
    'Google português do Brasil 3 (Natural)':
        {natureName: 'Flor', description: 'Voz doce, tom médio'},
  },
  'pt-pt': {
    'Google português de Portugal 1 (Natural)':
        {natureName: 'Baía', description: 'Voz calma, tom médio'},
    'Google português de Portugal 2 (Natural)':
        {natureName: 'Brisa', description: 'Voz relaxada, tom baixo'},
    'Google português de Portugal 3 (Natural)':
        {natureName: 'Oceano', description: 'Voz viva, tom baixo'},
    'Google português de Portugal 4 (Natural)':
        {natureName: 'Ribeiro', description: 'Voz tranquila, tom médio'},
  },
  'si-lk': {
    'Google සිංහල (Natural)':
        {natureName: 'ගංගා', description: 'සන්සුන් ස්වරය, මධ්‍යම තාරතාව'},
  },
  'sk-sk': {
    'Google Slovenčina (Natural)':
        {natureName: 'Úsvit', description: 'Čistý hlas, stredná výška tónu'},
  },
  'sv-se': {
    'Google Svenska 1 (Natural)':
        {natureName: 'Morgon', description: 'Varm ton, medium stämma'},
    'Google Svenska 2 (Natural)':
        {natureName: 'Mossa', description: 'Mjuk ton, medium stämma'},
    'Google Svenska 3 (Natural)':
        {natureName: 'Bäck', description: 'Ljus ton, medium stämma'},
    'Google Svenska 4 (Natural)':
        {natureName: 'Sund', description: 'Vänlig ton, mörk stämma'},
    'Google Svenska 5 (Natural)':
        {natureName: 'Vind', description: 'Upprymd ton, mörk stämma'},
  },
  'th-th': {
    'Google ไทย (Natural)':
        {natureName: 'ทะเล', description: 'เสียงโทนสดใส ระดับเสียงปานกลาง'},
    // ChromeOS-only: this key has no Google identifier, so this mapping
    // is unreachable on other platforms.
    'Chrome OS ไทย 2':
        {natureName: 'ทราย', description: 'เสียงโทนนุ่มนวล ระดับเสียงต่ำ'},
  },
  'tr-tr': {
    'Google Türkçe 1 (Natural)':
        {natureName: 'Çiçek', description: 'Yumuşak ton, orta perde'},
    'Google Türkçe 2 (Natural)':
        {natureName: 'Gece', description: 'Huzurlu ton, düşük perde'},
    'Google Türkçe 3 (Natural)':
        {natureName: 'Çimen', description: 'Vurgulu ton, orta perde'},
    'Google Türkçe 4 (Natural)':
        {natureName: 'Yosun', description: 'Sakin ton, orta perde'},
    'Google Türkçe 5 (Natural)':
        {natureName: 'Çınar', description: 'Dinlendirici ton, düşük perde'},
  },
  'uk-ua': {
    'Google українська (Natural)':
        {natureName: 'Літепло', description: 'Теплий тон, середня висота'},
  },
  'vi-vn': {
    'Google Tiếng Việt 1 (Natural)':
        {natureName: 'Bông', description: 'Nhẹ nhàng, cao độ trung bình'},
    'Google Tiếng Việt 2 (Natural)':
        {natureName: 'Hạ', description: 'Tươi sáng, cao độ trung bình'},
    'Google Tiếng Việt 3 (Natural)':
        {natureName: 'Bình', description: 'Êm dịu, cao độ thấp'},
    'Google Tiếng Việt 4 (Natural)':
        {natureName: 'Mộc', description: 'Điềm đạm, cao độ trung bình'},
    'Google Tiếng Việt 5 (Natural)':
        {natureName: 'Nhung', description: 'Ấm áp, cao độ thấp'},
  },
  // ChromeOS-only: these keys have no Google identifier, so these mappings
  // are unreachable on other platforms.
  'yue-hk': {
    'Chrome OS 粵語 1': {natureName: '卵石', description: '清晰的音調，中音'},
    'Chrome OS 粵語 2': {natureName: '松針', description: '清脆的音調，中音'},
    'Chrome OS 粵語 3': {natureName: '溪澗', description: '明快的音調，低音'},
    'Chrome OS 粵語 4': {natureName: '晴空', description: '明朗的音調，中音'},
    'Chrome OS 粵語 5': {natureName: '山谷', description: '爽朗的音調，低音'},
  },
};
// LINT.ThenChange(voice_language_conversions.ts:AVAILABLE_GOOGLE_TTS_LOCALES)

// Flattened lookup table. Keys are exact engine names, case-sensitive.
const VOICE_NATURE_NAMING_TABLE: ReadonlyMap<string, VoiceNatureNaming> =
    new Map(Object.values(VOICE_NATURE_NAMING_BY_LOCALE)
                .flatMap(entries => Object.entries(entries)));

// Returns the nature naming for `engineVoiceName` (null when the voice has
// no mapping). The key is the exact `SpeechSynthesisVoice.name`.
export function getVoiceNatureNaming(engineVoiceName: string):
    VoiceNatureNaming|null {
  return VOICE_NATURE_NAMING_TABLE.get(engineVoiceName) ?? null;
}
