// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class is responsible for creating the header's mediator and property model change
 * processor, and managing communication with other coordinators.
 */
@NullMarked
public class DocumentPictureInPictureHeaderCoordinator {
    private final DocumentPictureInPictureHeaderMediator mMediator;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Constructor for the Document Picture-in-Picture (PiP) header coordinator.
     *
     * @param view The view to be used for the header.
     * @param desktopWindowStateManager The desktop window state manager to observe for app header
     *     state changes.
     * @param themeColorProvider The theme color provider to observe for theme color changes.
     */
    public DocumentPictureInPictureHeaderCoordinator(
            View view,
            DesktopWindowStateManager desktopWindowStateManager,
            ThemeColorProvider themeColorProvider,
            Context context,
            DocumentPictureInPictureHeaderDelegate delegate,
            boolean isBackToTabShown,
            WebContents openerWebContents,
            WebContents webContents) {
        PropertyModel model =
                new PropertyModel.Builder(DocumentPictureInPictureHeaderProperties.ALL_KEYS)
                        .build();
        mMediator =
                new DocumentPictureInPictureHeaderMediator(
                        model,
                        desktopWindowStateManager,
                        themeColorProvider,
                        context,
                        delegate,
                        isBackToTabShown,
                        openerWebContents,
                        webContents);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, view, DocumentPictureInPictureHeaderViewBinder::bind);
    }

    public void destroy() {
        mMediator.destroy();
        mPropertyModelChangeProcessor.destroy();
    }
}
