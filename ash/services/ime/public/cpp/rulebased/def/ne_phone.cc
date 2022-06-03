// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ne_phone.h"

namespace ne_phone {

const char* kId = "ne_phone";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u093c",  // BackQuote
    u8"\u0967",  // Digit1
    u8"\u0968",  // Digit2
    u8"\u0969",  // Digit3
    u8"\u096a",  // Digit4
    u8"\u096b",  // Digit5
    u8"\u096c",  // Digit6
    u8"\u096d",  // Digit7
    u8"\u096e",  // Digit8
    u8"\u096f",  // Digit9
    u8"\u0966",  // Digit0
    u8"-",       // Minus
    u8"\u200d",  // Equal
    u8"\u091f",  // KeyQ
    u8"\u094c",  // KeyW
    u8"\u0947",  // KeyE
    u8"\u0930",  // KeyR
    u8"\u0924",  // KeyT
    u8"\u092f",  // KeyY
    u8"\u0941",  // KeyU
    u8"\u093f",  // KeyI
    u8"\u094b",  // KeyO
    u8"\u092a",  // KeyP
    u8"\u0907",  // BracketLeft
    u8"\u090f",  // BracketRight
    u8"\u0950",  // Backslash
    u8"\u093e",  // KeyA
    u8"\u0938",  // KeyS
    u8"\u0926",  // KeyD
    u8"\u0909",  // KeyF
    u8"\u0917",  // KeyG
    u8"\u0939",  // KeyH
    u8"\u091c",  // KeyJ
    u8"\u0915",  // KeyK
    u8"\u0932",  // KeyL
    u8";",       // Semicolon
    u8"'",       // Quote
    u8"\u0937",  // KeyZ
    u8"\u0921",  // KeyX
    u8"\u091a",  // KeyC
    u8"\u0935",  // KeyV
    u8"\u092c",  // KeyB
    u8"\u0928",  // KeyN
    u8"\u092e",  // KeyM
    u8",",       // Comma
    u8"\u0964",  // Period
    u8"\u094d",  // Slash
    u8"\u0020",  // Space
};
const char* kShift[] = {
    u8"\u093d",         // BackQuote
    u8"!",              // Digit1
    u8"@",              // Digit2
    u8"#",              // Digit3
    u8"\u0930\u0941.",  // Digit4
    u8"%",              // Digit5
    u8"^",              // Digit6
    u8"&",              // Digit7
    u8"*",              // Digit8
    u8"(",              // Digit9
    u8")",              // Digit0
    u8"_",              // Minus
    u8"\u200c",         // Equal
    u8"\u0920",         // KeyQ
    u8"\u0914",         // KeyW
    u8"\u0948",         // KeyE
    u8"\u0943",         // KeyR
    u8"\u0925",         // KeyT
    u8"\u091e",         // KeyY
    u8"\u0942",         // KeyU
    u8"\u0940",         // KeyI
    u8"\u0913",         // KeyO
    u8"\u092b",         // KeyP
    u8"\u0908",         // BracketLeft
    u8"\u0910",         // BracketRight
    u8"\u0903",         // Backslash
    u8"\u0906",         // KeyA
    u8"\u0936",         // KeyS
    u8"\u0927",         // KeyD
    u8"\u090a",         // KeyF
    u8"\u0918",         // KeyG
    u8"\u0905",         // KeyH
    u8"\u091d",         // KeyJ
    u8"\u0916",         // KeyK
    u8"\u0965",         // KeyL
    u8":",              // Semicolon
    u8"\"",             // Quote
    u8"\u090b",         // KeyZ
    u8"\u0922",         // KeyX
    u8"\u091b",         // KeyC
    u8"\u0901",         // KeyV
    u8"\u092d",         // KeyB
    u8"\u0923",         // KeyN
    u8"\u0902",         // KeyM
    u8"\u0919",         // Comma
    u8".",              // Period
    u8"?",              // Slash
    u8"\u0020",         // Space
};
const char* kCapslock[] = {
    u8"\u093c",  // BackQuote
    u8"\u0967",  // Digit1
    u8"\u0968",  // Digit2
    u8"\u0969",  // Digit3
    u8"\u096a",  // Digit4
    u8"\u096b",  // Digit5
    u8"\u096c",  // Digit6
    u8"\u096d",  // Digit7
    u8"\u096e",  // Digit8
    u8"\u096f",  // Digit9
    u8"\u0966",  // Digit0
    u8"-",       // Minus
    u8"\u200d",  // Equal
    u8"\u091f",  // KeyQ
    u8"\u094c",  // KeyW
    u8"\u0947",  // KeyE
    u8"\u0930",  // KeyR
    u8"\u0924",  // KeyT
    u8"\u092f",  // KeyY
    u8"\u0941",  // KeyU
    u8"\u093f",  // KeyI
    u8"\u094b",  // KeyO
    u8"\u092a",  // KeyP
    u8"\u0907",  // BracketLeft
    u8"\u090f",  // BracketRight
    u8"\u0950",  // Backslash
    u8"\u093e",  // KeyA
    u8"\u0938",  // KeyS
    u8"\u0926",  // KeyD
    u8"\u0909",  // KeyF
    u8"\u0917",  // KeyG
    u8"\u0939",  // KeyH
    u8"\u091c",  // KeyJ
    u8"\u0915",  // KeyK
    u8"\u0932",  // KeyL
    u8";",       // Semicolon
    u8"'",       // Quote
    u8"\u0937",  // KeyZ
    u8"\u0921",  // KeyX
    u8"\u091a",  // KeyC
    u8"\u0935",  // KeyV
    u8"\u092c",  // KeyB
    u8"\u0928",  // KeyN
    u8"\u092e",  // KeyM
    u8",",       // Comma
    u8"\u0964",  // Period
    u8"\u094d",  // Slash
    u8"\u0020",  // Space
};
const char* kShiftCapslock[] = {
    u8"\u093d",         // BackQuote
    u8"!",              // Digit1
    u8"@",              // Digit2
    u8"#",              // Digit3
    u8"\u0930\u0941.",  // Digit4
    u8"%",              // Digit5
    u8"^",              // Digit6
    u8"&",              // Digit7
    u8"*",              // Digit8
    u8"(",              // Digit9
    u8")",              // Digit0
    u8"_",              // Minus
    u8"\u200c",         // Equal
    u8"\u0920",         // KeyQ
    u8"\u0914",         // KeyW
    u8"\u0948",         // KeyE
    u8"\u0943",         // KeyR
    u8"\u0925",         // KeyT
    u8"\u091e",         // KeyY
    u8"\u0942",         // KeyU
    u8"\u0940",         // KeyI
    u8"\u0913",         // KeyO
    u8"\u092b",         // KeyP
    u8"\u0908",         // BracketLeft
    u8"\u0910",         // BracketRight
    u8"\u0903",         // Backslash
    u8"\u0906",         // KeyA
    u8"\u0936",         // KeyS
    u8"\u0927",         // KeyD
    u8"\u090a",         // KeyF
    u8"\u0918",         // KeyG
    u8"\u0905",         // KeyH
    u8"\u091d",         // KeyJ
    u8"\u0916",         // KeyK
    u8"\u0965",         // KeyL
    u8":",              // Semicolon
    u8"\"",             // Quote
    u8"\u090b",         // KeyZ
    u8"\u0922",         // KeyX
    u8"\u091b",         // KeyC
    u8"\u0901",         // KeyV
    u8"\u092d",         // KeyB
    u8"\u0923",         // KeyN
    u8"\u0902",         // KeyM
    u8"\u0919",         // Comma
    u8".",              // Period
    u8"?",              // Slash
    u8"\u0020",         // Space
};
const char** kKeyMap[8] = {kNormal,   kShift,        kNormal,
                           kShift,    kCapslock,     kShiftCapslock,
                           kCapslock, kShiftCapslock};

}  // namespace ne_phone
