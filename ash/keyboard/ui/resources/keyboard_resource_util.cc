// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/resources/keyboard_resource_util.h"

#include "ash/keyboard/ui/grit/keyboard_resources.h"
#include "ash/keyboard/ui/grit/keyboard_resources_map.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace keyboard {

const char kKeyboardURL[] = "chrome://keyboard";
const char kKeyboardHost[] = "keyboard";

const GritResourceMap* GetKeyboardExtensionResources(size_t* size) {
  // This looks a lot like the contents of a resource map; however it is
  // necessary to have a custom path for the extension path, so the resource
  // map cannot be used directly.
  static const GritResourceMap kKeyboardResources[] = {
      {"keyboard/locales/en.js", IDR_KEYBOARD_LOCALES_EN},
      {"keyboard/config/emoji.js", IDR_KEYBOARD_CONFIG_EMOJI},
      {"keyboard/config/hwt.js", IDR_KEYBOARD_CONFIG_HWT},
      {"keyboard/config/us.js", IDR_KEYBOARD_CONFIG_US},
      {"keyboard/emoji.css", IDR_KEYBOARD_CSS_EMOJI},
      {"keyboard/images/3dots.png", IDR_KEYBOARD_IMAGES_3_DOTS},
      {"keyboard/images/back_to_keyboard.png",
       IDR_KEYBOARD_IMAGES_BACK_TO_KEYBOARD},
      {"keyboard/images/backspace.png", IDR_KEYBOARD_IMAGES_BACKSPACE},
      {"keyboard/images/car.png", IDR_KEYBOARD_IMAGES_CAR},
      {"keyboard/images/check.png", IDR_KEYBOARD_IMAGES_CHECK},
      {"keyboard/images/check_in_menu.png", IDR_KEYBOARD_IMAGES_CHECK_IN_MENU},
      {"keyboard/images/compact.png", IDR_KEYBOARD_IMAGES_COMPACT},
      {"keyboard/images/down.png", IDR_KEYBOARD_IMAGES_DOWN},
      {"keyboard/images/emoji.png", IDR_KEYBOARD_IMAGES_EMOJI},
      {"keyboard/images/emoji_car.png", IDR_KEYBOARD_IMAGES_EMOJI_CAR},
      {"keyboard/images/emoji_crown.png", IDR_KEYBOARD_IMAGES_EMOJI_CROWN},
      {"keyboard/images/emoji_emoticon.png",
       IDR_KEYBOARD_IMAGES_EMOJI_EMOTICON},
      {"keyboard/images/emoji_flower.png", IDR_KEYBOARD_IMAGES_EMOJI_FLOWER},
      {"keyboard/images/emoji_hot.png", IDR_KEYBOARD_IMAGES_EMOJI_HOT},
      {"keyboard/images/emoji_recent.png", IDR_KEYBOARD_IMAGES_EMOJI_RECENT},
      {"keyboard/images/emoji_shape.png", IDR_KEYBOARD_IMAGES_EMOJI_SHAPE},
      {"keyboard/images/emoji_cat_items.png", IDR_KEYBOARD_IMAGES_CAT},
      {"keyboard/images/emoticon.png", IDR_KEYBOARD_IMAGES_EMOTICON},
      {"keyboard/images/enter.png", IDR_KEYBOARD_IMAGES_RETURN},
      {"keyboard/images/error.png", IDR_KEYBOARD_IMAGES_ERROR},
      {"keyboard/images/favorit.png", IDR_KEYBOARD_IMAGES_FAVORITE},
      {"keyboard/images/flower.png", IDR_KEYBOARD_IMAGES_FLOWER},
      {"keyboard/images/globe.png", IDR_KEYBOARD_IMAGES_GLOBE},
      {"keyboard/images/hide.png", IDR_KEYBOARD_IMAGES_HIDE},
      {"keyboard/images/hidekeyboard.png", IDR_KEYBOARD_IMAGES_HIDE_KEYBOARD},
      {"keyboard/images/keyboard.svg", IDR_KEYBOARD_IMAGES_KEYBOARD},
      {"keyboard/images/left.png", IDR_KEYBOARD_IMAGES_LEFT},
      {"keyboard/images/penci.png", IDR_KEYBOARD_IMAGES_PENCIL},
      {"keyboard/images/recent.png", IDR_KEYBOARD_IMAGES_RECENT},
      {"keyboard/images/regular_size.png", IDR_KEYBOARD_IMAGES_FULLSIZE},
      {"keyboard/images/menu.png", IDR_KEYBOARD_IMAGES_MENU},
      {"keyboard/images/pencil.png", IDR_KEYBOARD_IMAGES_PENCIL},
      {"keyboard/images/right.png", IDR_KEYBOARD_IMAGES_RIGHT},
      {"keyboard/images/search.png", IDR_KEYBOARD_IMAGES_SEARCH},
      {"keyboard/images/select_right.png", IDR_KEYBOARD_IMAGES_SELECT_RIGHT},
      {"keyboard/images/select_left.png", IDR_KEYBOARD_IMAGES_SELECT_LEFT},
      {"keyboard/images/setting.png", IDR_KEYBOARD_IMAGES_SETTINGS},
      {"keyboard/images/shift.png", IDR_KEYBOARD_IMAGES_SHIFT},
      {"keyboard/images/space.png", IDR_KEYBOARD_IMAGES_SPACE},
      {"keyboard/images/tab.png", IDR_KEYBOARD_IMAGES_TAB},
      {"keyboard/images/tab_in_fullsize.png",
       IDR_KEYBOARD_IMAGES_TAB_IN_FULLSIZE},
      {"keyboard/images/triangle.png", IDR_KEYBOARD_IMAGES_TRIANGLE},
      {"keyboard/images/up.png", IDR_KEYBOARD_IMAGES_UP},
      {"keyboard/index.html", IDR_KEYBOARD_INDEX},
      {"keyboard/inputview_adapter.js", IDR_KEYBOARD_INPUTVIEW_ADAPTER},
      {"keyboard/inputview.css", IDR_KEYBOARD_INPUTVIEW_CSS},
      {"keyboard/inputview.js", IDR_KEYBOARD_INPUTVIEW_JS},
      {"keyboard/inputview_layouts/101kbd.js", IDR_KEYBOARD_LAYOUTS_101},
      {"keyboard/inputview_layouts/compactkbd-qwerty.js",
       IDR_KEYBOARD_LAYOUTS_COMPACT_QWERTY},
      {"keyboard/inputview_layouts/compactkbd-numberpad.js",
       IDR_KEYBOARD_LAYOUTS_COMPACT_NUMBERPAD},
      {"keyboard/inputview_layouts/emoji.js", IDR_KEYBOARD_LAYOUTS_EMOJI},
      {"keyboard/inputview_layouts/handwriting.js", IDR_KEYBOARD_LAYOUTS_HWT},
      {"keyboard/manifest.json", IDR_KEYBOARD_MANIFEST},
      {"keyboard/sounds/keypress-delete.wav",
       IDR_KEYBOARD_SOUNDS_KEYPRESS_DELETE},
      {"keyboard/sounds/keypress-return.wav",
       IDR_KEYBOARD_SOUNDS_KEYPRESS_RETURN},
      {"keyboard/sounds/keypress-spacebar.wav",
       IDR_KEYBOARD_SOUNDS_KEYPRESS_SPACEBAR},
      {"keyboard/sounds/keypress-standard.wav",
       IDR_KEYBOARD_SOUNDS_KEYPRESS_STANDARD},
  };
  *size = base::size(kKeyboardResources);
  return kKeyboardResources;
}

void InitializeKeyboardResources() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;

  base::FilePath pak_dir;
  base::PathService::Get(base::DIR_MODULE, &pak_dir);
  base::FilePath pak_file =
      pak_dir.Append(FILE_PATH_LITERAL("keyboard_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_100P);
}

}  // namespace keyboard
