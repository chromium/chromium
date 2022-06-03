// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ckb_ar.h"

namespace ckb_ar {

const char* kId = "ckb_ar";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u0698",        // BackQuote
    u8"\u0661",        // Digit1
    u8"\u0662",        // Digit2
    u8"\u0663",        // Digit3
    u8"\u0664",        // Digit4
    u8"\u0665",        // Digit5
    u8"\u0666",        // Digit6
    u8"\u0667",        // Digit7
    u8"\u0668",        // Digit8
    u8"\u0669",        // Digit9
    u8"\u0660",        // Digit0
    u8"-",             // Minus
    u8"=",             // Equal
    u8"\u0686",        // KeyQ
    u8"\u0635",        // KeyW
    u8"\u067e",        // KeyE
    u8"\u0642",        // KeyR
    u8"\u0641",        // KeyT
    u8"\u063a",        // KeyY
    u8"\u0639",        // KeyU
    u8"\u0647",        // KeyI
    u8"\u062e",        // KeyO
    u8"\u062d",        // KeyP
    u8"\u062c",        // BracketLeft
    u8"\u062f",        // BracketRight
    u8"\\",            // Backslash
    u8"\u0634",        // KeyA
    u8"\u0633",        // KeyS
    u8"\u06cc",        // KeyD
    u8"\u0628",        // KeyF
    u8"\u0644",        // KeyG
    u8"\u0627",        // KeyH
    u8"\u062a",        // KeyJ
    u8"\u0646",        // KeyK
    u8"\u0645",        // KeyL
    u8"\u06a9",        // Semicolon
    u8"\u06af",        // Quote
    u8"\u0626",        // KeyZ
    u8"\u0621",        // KeyX
    u8"\u06c6",        // KeyC
    u8"\u0631",        // KeyV
    u8"\u0644\u0627",  // KeyB
    u8"\u0649",        // KeyN
    u8"\u0647\u200c",  // KeyM
    u8"\u0648",        // Comma
    u8"\u0632",        // Period
    u8"/",             // Slash
    u8"\u0020",        // Space
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
    u8"\u0636",        // KeyQ
    u8"}",             // KeyW
    u8"\u062b",        // KeyE
    u8"{",             // KeyR
    u8"\u06a4",        // KeyT
    u8"\u0625",        // KeyY
    u8"",              // KeyU
    u8"'",             // KeyI
    u8"\"",            // KeyO
    u8"\u061b",        // KeyP
    u8">",             // BracketLeft
    u8"<",             // BracketRight
    u8"|",             // Backslash
    u8"]",             // KeyA
    u8"[",             // KeyS
    u8"\u06ce",        // KeyD
    u8"",              // KeyF
    u8"\u06b5",        // KeyG
    u8"\u0623",        // KeyH
    u8"\u0640",        // KeyJ
    u8"\u060c",        // KeyK
    u8"/",             // KeyL
    u8":",             // Semicolon
    u8"\u0637",        // Quote
    u8"\u2904",        // KeyZ
    u8"\u0648\u0648",  // KeyX
    u8"\u0624",        // KeyC
    u8"\u0695",        // KeyV
    u8"\u06b5\u0627",  // KeyB
    u8"\u0622",        // KeyN
    u8"\u0629",        // KeyM
    u8",",             // Comma
    u8".",             // Period
    u8"\u061f",        // Slash
    u8"\u200c",        // Space
};
const char** kKeyMap[8] = {kNormal, kShift, kNormal, kShift,
                           kNormal, kShift, kNormal, kShift};

}  // namespace ckb_ar
