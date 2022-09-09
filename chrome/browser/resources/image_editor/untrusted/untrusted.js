// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {EditAction, ImageEditorHandler} from './image_editor.mojom-webui.js';


const imageEditorHandler = ImageEditorHandler.getRemote();

// Log user action that placeholder webui has loaded.
imageEditorHandler.recordUserAction(EditAction.kFallbackLaunched);
