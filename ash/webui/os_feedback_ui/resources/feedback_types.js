// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import '//resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import '//resources/mojo/url/mojom/url.mojom-lite.js';
import './mojom/os_feedback_ui.mojom-lite.js';

/**
 * Type alias for HelpContentType.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentType}
 */
export const HelpContentType = ash.osFeedbackUi.mojom.HelpContentType;

/**
 * Type alias for HelpContent.
 * @typedef {ash.osFeedbackUi.mojom.HelpContent}
 */
export const HelpContent = ash.osFeedbackUi.mojom.HelpContent;

/**
 * Type alias for an array of HelpContent.
 * @typedef !Array<!HelpContent>
 */
export let HelpContentList;

/**
 * Type alias for SearchRequest.
 * @typedef {ash.osFeedbackUi.mojom.SearchRequest}
 */
export const SearchRequest = ash.osFeedbackUi.mojom.SearchRequest;

/**
 * Type alias for SearchResponse.
 * @typedef {ash.osFeedbackUi.mojom.SearchResponse}
 */
export const SearchResponse = ash.osFeedbackUi.mojom.SearchResponse;

/**
 * Type alias for the HelpContentProviderInterface.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentProviderInterface}
 */
export const HelpContentProviderInterface =
    ash.osFeedbackUi.mojom.HelpContentProviderInterface;

/**
 * Type alias for the HelpContentProvider.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentProvider}
 */
export const HelpContentProvider = ash.osFeedbackUi.mojom.HelpContentProvider;
