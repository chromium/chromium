// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {EditAction, ImageEditorHandler} from './image_editor.mojom-webui.js';


const imageEditorHandler = ImageEditorHandler.getRemote();

// Log user action that placeholder webui has loaded.
imageEditorHandler.recordUserAction(EditAction.kFallbackLaunched);
