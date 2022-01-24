// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ckb_en.h"

namespace ckb_en {

const char* kId = "ckb_en";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u20ac",  // BackQuote
    u8"\u0661",  // Digit1
    u8"\u0662",  // Digit2
    u8"\u0663",  // Digit3
    u8"\u0664",  // Digit4
    u8"\u0665",  // Digit5
    u8"\u0666",  // Digit6
    u8"\u0667",  // Digit7
    u8"\u0668",  // Digit8
    u8"\u0669",  // Digit9
    u8"\u0660",  // Digit0
    u8"-",       // Minus
    u8"=",       // Equal
    u8"\u0642",  // KeyQ
    u8"\u0648",  // KeyW
    u8"\u06d5",  // KeyE
    u8"\u0631",  // KeyR
    u8"\u062a",  // KeyT
    u8"\u06cc",  // KeyY
    u8"\u0626",  // KeyU
    u8"\u062d",  // KeyI
    u8"\u06c6",  // KeyO
    u8"\u067e",  // KeyP
    u8"}",       // BracketLeft
    u8"{",       // BracketRight
    u8"\\",      // Backslash
    u8"\u0627",  // KeyA
    u8"\u0633",  // KeyS
    u8"\u062f",  // KeyD
    u8"\u0641",  // KeyF
    u8"\u06af",  // KeyG
    u8"\u0647",  // KeyH
    u8"\u0698",  // KeyJ
    u8"\u06a9",  // KeyK
    u8"\u0644",  // KeyL
    u8"\u061b",  // Semicolon
    u8"'",       // Quote
    u8"\u0632",  // KeyZ
    u8"\u062e",  // KeyX
    u8"\u062c",  // KeyC
    u8"\u06a4",  // KeyV
    u8"\u0628",  // KeyB
    u8"\u0646",  // KeyN
    u8"\u0645",  // KeyM
    u8"\u060c",  // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char* kShift[] = {
    u8"~",             // BackQuote
    u8"!",             // Digit1
    u8"@",             // Digit2
    u8"#",             // Digit3
    u8"$",             // Digit4
    u8"%",             // Digit5
    u8"\u00bb",        // Digit6
    u8"\u00ab",        // Digit7
    u8"*",             // Digit8
    u8")",             // Digit9
    u8"(",             // Digit0
    u8"_",             // Minus
    u8"+",             // Equal
    u8"`",             // KeyQ
    u8"\u0648\u0648",  // KeyW
    u8"\u064a",        // KeyE
    u8"\u0695",        // KeyR
    u8"\u0637",        // KeyT
    u8"\u06ce",        // KeyY
    u8"\u0621",        // KeyU
    u8"\u0639",        // KeyI
    u8"\u0624",        // KeyO
    u8"\u062b",        // KeyP
    u8"]",             // BracketLeft
    u8"[",             // BracketRight
    u8"|",             // Backslash
    u8"\u0622",        // KeyA
    u8"\u0634",        // KeyS
    u8"\u0630",        // KeyD
    u8"\u0625",        // KeyF
    u8"\u063a",        // KeyG
    u8"\u200c",        // KeyH
    u8"\u0623",        // KeyJ
    u8"\u0643",        // KeyK
    u8"\u06b5",        // KeyL
    u8":",             // Semicolon
    u8"\"",            // Quote
    u8"\u0636",        // KeyZ
    u8"\u0635",        // KeyX
    u8"\u0686",        // KeyC
    u8"\u0638",        // KeyV
    u8"\u0649",        // KeyB
    u8"\u0629",        // KeyN
    u8"\u0640",        // KeyM
    u8">",             // Comma
    u8"<",             // Period
    u8"\u061f",        // Slash
    u8"\u200c",        // Space
};
const char** kKeyMap[8] = {kNormal, kShift, kNormal, kShift,
                           kNormal, kShift, kNormal, kShift};

}  // namespace ckb_en
