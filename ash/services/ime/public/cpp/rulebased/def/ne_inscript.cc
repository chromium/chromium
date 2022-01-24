// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ne_inscript.h"

namespace ne_inscript {

const char* kId = "ne_inscript";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u091e",              // BackQuote
    u8"\u0967",              // Digit1
    u8"\u0968",              // Digit2
    u8"\u0969",              // Digit3
    u8"\u096a",              // Digit4
    u8"\u096b",              // Digit5
    u8"\u096c",              // Digit6
    u8"\u096d",              // Digit7
    u8"\u096e",              // Digit8
    u8"\u096f",              // Digit9
    u8"\u0966",              // Digit0
    u8"\u0914",              // Minus
    u8"\u200d",              // Equal
    u8"\u0924\u094d\u0930",  // KeyQ
    u8"\u0927",              // KeyW
    u8"\u092d",              // KeyE
    u8"\u091a",              // KeyR
    u8"\u0924",              // KeyT
    u8"\u0925",              // KeyY
    u8"\u0917",              // KeyU
    u8"\u0937",              // KeyI
    u8"\u092f",              // KeyO
    u8"\u0909",              // KeyP
    u8"\u0930\u094d",        // BracketLeft
    u8"\u0947",              // BracketRight
    u8"\u094d",              // Backslash
    u8"\u092c",              // KeyA
    u8"\u0915",              // KeyS
    u8"\u092e",              // KeyD
    u8"\u093e",              // KeyF
    u8"\u0928",              // KeyG
    u8"\u091c",              // KeyH
    u8"\u0935",              // KeyJ
    u8"\u092a",              // KeyK
    u8"\u093f",              // KeyL
    u8"\u0938",              // Semicolon
    u8"\u0941",              // Quote
    u8"\u0936",              // KeyZ
    u8"\u0939",              // KeyX
    u8"\u0905",              // KeyC
    u8"\u0916",              // KeyV
    u8"\u0926",              // KeyB
    u8"\u0932",              // KeyN
    u8"\u0903",              // KeyM
    u8"\u093d",              // Comma
    u8"\u0964",              // Period
    u8"\u0930",              // Slash
    u8"\u0020",              // Space
};
const char* kShift[] = {
    u8"\u0965",              // BackQuote
    u8"\u091c\u094d\u091e",  // Digit1
    u8"\u0908",              // Digit2
    u8"\u0918",              // Digit3
    u8"$",                   // Digit4
    u8"\u091b",              // Digit5
    u8"\u091f",              // Digit6
    u8"\u0920",              // Digit7
    u8"\u0921",              // Digit8
    u8"\u0922",              // Digit9
    u8"\u0923",              // Digit0
    u8"\u0913",              // Minus
    u8"\u200c",              // Equal
    u8"\u0924\u094d\u0924",  // KeyQ
    u8"\u0921\u094d\u0922",  // KeyW
    u8"\u0910",              // KeyE
    u8"\u0926\u094d\u0935",  // KeyR
    u8"\u091f\u094d\u091f",  // KeyT
    u8"\u0920\u094d\u0920",  // KeyY
    u8"\u090a",              // KeyU
    u8"\u0915\u094d\u0937",  // KeyI
    u8"\u0907",              // KeyO
    u8"\u090f",              // KeyP
    u8"\u0943",              // BracketLeft
    u8"\u0948",              // BracketRight
    u8"\u0902",              // Backslash
    u8"\u0906",              // KeyA
    u8"\u0919\u094d\u0915",  // KeyS
    u8"\u0919\u094d\u0917",  // KeyD
    u8"\u0901",              // KeyF
    u8"\u0926\u094d\u0926",  // KeyG
    u8"\u091d",              // KeyH
    u8"\u094b",              // KeyJ
    u8"\u092b",              // KeyK
    u8"\u0940",              // KeyL
    u8"\u091f\u094d\u0920",  // Semicolon
    u8"\u0942",              // Quote
    u8"\u0915\u094d\u0915",  // KeyZ
    u8"\u0939\u094d\u092e",  // KeyX
    u8"\u090b",              // KeyC
    u8"\u0950",              // KeyV
    u8"\u094c",              // KeyB
    u8"\u0926\u094d\u092f",  // KeyN
    u8"\u0921\u094d\u0921",  // KeyM
    u8"\u0919",              // Comma
    u8"\u0936\u094d\u0930",  // Period
    u8"\u0930\u0941",        // Slash
    u8"\u0020",              // Space
};
const char* kCapslock[] = {
    u8"\u091e",              // BackQuote
    u8"\u0967",              // Digit1
    u8"\u0968",              // Digit2
    u8"\u0969",              // Digit3
    u8"\u096a",              // Digit4
    u8"\u096b",              // Digit5
    u8"\u096c",              // Digit6
    u8"\u096d",              // Digit7
    u8"\u096e",              // Digit8
    u8"\u096f",              // Digit9
    u8"\u0966",              // Digit0
    u8"\u0914",              // Minus
    u8"\u200d",              // Equal
    u8"\u0924\u094d\u0930",  // KeyQ
    u8"\u0927",              // KeyW
    u8"\u092d",              // KeyE
    u8"\u091a",              // KeyR
    u8"\u0924",              // KeyT
    u8"\u0925",              // KeyY
    u8"\u0917",              // KeyU
    u8"\u0937",              // KeyI
    u8"\u092f",              // KeyO
    u8"\u0909",              // KeyP
    u8"\u0930\u094d",        // BracketLeft
    u8"\u0947",              // BracketRight
    u8"\u094d",              // Backslash
    u8"\u092c",              // KeyA
    u8"\u0915",              // KeyS
    u8"\u092e",              // KeyD
    u8"\u093e",              // KeyF
    u8"\u0928",              // KeyG
    u8"\u091c",              // KeyH
    u8"\u0935",              // KeyJ
    u8"\u092a",              // KeyK
    u8"\u093f",              // KeyL
    u8"\u0938",              // Semicolon
    u8"\u0941",              // Quote
    u8"\u0936",              // KeyZ
    u8"\u0939",              // KeyX
    u8"\u0905",              // KeyC
    u8"\u0916",              // KeyV
    u8"\u0926",              // KeyB
    u8"\u0932",              // KeyN
    u8"\u0903",              // KeyM
    u8"\u093d",              // Comma
    u8"\u0964",              // Period
    u8"\u0930",              // Slash
    u8"\u0020",              // Space
};
const char* kShiftCapslock[] = {
    u8"\u0965",              // BackQuote
    u8"\u091c\u094d\u091e",  // Digit1
    u8"\u0908",              // Digit2
    u8"\u0918",              // Digit3
    u8"$",                   // Digit4
    u8"\u091b",              // Digit5
    u8"\u091f",              // Digit6
    u8"\u0920",              // Digit7
    u8"\u0921",              // Digit8
    u8"\u0922",              // Digit9
    u8"\u0923",              // Digit0
    u8"\u0913",              // Minus
    u8"\u200c",              // Equal
    u8"\u0924\u094d\u0924",  // KeyQ
    u8"\u0921\u094d\u0922",  // KeyW
    u8"\u0910",              // KeyE
    u8"\u0926\u094d\u0935",  // KeyR
    u8"\u091f\u094d\u091f",  // KeyT
    u8"\u0920\u094d\u0920",  // KeyY
    u8"\u090a",              // KeyU
    u8"\u0915\u094d\u0937",  // KeyI
    u8"\u0907",              // KeyO
    u8"\u090f",              // KeyP
    u8"\u0943",              // BracketLeft
    u8"\u0948",              // BracketRight
    u8"\u0902",              // Backslash
    u8"\u0906",              // KeyA
    u8"\u0919\u094d\u0915",  // KeyS
    u8"\u0919\u094d\u0917",  // KeyD
    u8"\u0901",              // KeyF
    u8"\u0926\u094d\u0926",  // KeyG
    u8"\u091d",              // KeyH
    u8"\u094b",              // KeyJ
    u8"\u092b",              // KeyK
    u8"\u0940",              // KeyL
    u8"\u091f\u094d\u0920",  // Semicolon
    u8"\u0942",              // Quote
    u8"\u0915\u094d\u0915",  // KeyZ
    u8"\u0939\u094d\u092e",  // KeyX
    u8"\u090b",              // KeyC
    u8"\u0950",              // KeyV
    u8"\u094c",              // KeyB
    u8"\u0926\u094d\u092f",  // KeyN
    u8"\u0921\u094d\u0921",  // KeyM
    u8"\u0919",              // Comma
    u8"\u0936\u094d\u0930",  // Period
    u8"\u0930\u0941",        // Slash
    u8"\u0020",              // Space
};
const char** kKeyMap[8] = {kNormal,   kShift,        kNormal,
                           kShift,    kCapslock,     kShiftCapslock,
                           kCapslock, kShiftCapslock};

}  // namespace ne_inscript
