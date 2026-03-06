// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the add / edit site search dialog. */
@NullMarked
public class SiteSearchDialogCoordinator {
    private final Context mContext;
    private final TemplateUrlService mTemplateUrlService;
    private final SiteSearchDialogMediator mMediator;

    public SiteSearchDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            TemplateUrlService templateUrlService) {
        mContext = context;
        mTemplateUrlService = templateUrlService;
        mMediator = new SiteSearchDialogMediator(context, modalDialogManager, templateUrlService);
    }

    public void showAddDialog() {
        SiteSearchDialogSaveAction saveAction =
                (name, keyword, url) -> {
                    mTemplateUrlService.addSearchEngine(name, keyword, url);
                };
        PropertyModel dialogViewModel = createDialogViewModel(mMediator);
        // The mediator needs to be initialized before the ModelChangeProcessor is created, or the
        // callback handler (e.g. NameChanged) for the view will detect null.
        mMediator.initialize(dialogViewModel, saveAction, /* templateUrl= */ null);

        View customView = LayoutInflater.from(mContext).inflate(R.layout.site_search_dialog, null);
        PropertyModelChangeProcessor.create(
                dialogViewModel, customView, SiteSearchDialogViewBinder::bind);

        PropertyModel dialogModel = createAddDialogModel(customView, mMediator);
        mMediator.show(dialogModel);
    }

    public void showEditDialog(TemplateUrl templateUrl) {
        SiteSearchDialogSaveAction saveAction =
                (name, keyword, url) -> {
                    mTemplateUrlService.editSearchEngine(
                            templateUrl.getKeyword(), name, keyword, url);
                };
        PropertyModel dialogViewModel = createEditDialogViewModel(templateUrl, mMediator);
        // The mediator needs to be initialized before the ModelChangeProcessor is created, or the
        // callback handler (e.g. NameChanged) for the view will detect null.
        mMediator.initialize(dialogViewModel, saveAction, templateUrl);

        View customView = LayoutInflater.from(mContext).inflate(R.layout.site_search_dialog, null);
        PropertyModelChangeProcessor.create(
                dialogViewModel, customView, SiteSearchDialogViewBinder::bind);

        PropertyModel dialogModel = createEditDialogModel(customView, mMediator);
        mMediator.show(dialogModel);
    }

    public void dismiss() {
        mMediator.dismiss();
    }

    private PropertyModel createDialogViewModel(SiteSearchDialogMediator mediator) {
        return new PropertyModel.Builder(SiteSearchDialogProperties.ALL_KEYS)
                .with(SiteSearchDialogProperties.ON_NAME_CHANGED, mediator::onNameChanged)
                .with(SiteSearchDialogProperties.ON_KEYWORD_CHANGED, mediator::onKeywordChanged)
                .with(SiteSearchDialogProperties.ON_URL_CHANGED, mediator::onUrlChanged)
                .build();
    }

    private PropertyModel createEditDialogViewModel(
            TemplateUrl templateUrl, SiteSearchDialogMediator mediator) {
        PropertyModel model = createDialogViewModel(mediator);
        model.set(SiteSearchDialogProperties.NAME, templateUrl.getShortName());
        model.set(SiteSearchDialogProperties.KEYWORD, templateUrl.getKeyword());
        model.set(SiteSearchDialogProperties.URL, templateUrl.getURL());

        boolean isUrlEditable = templateUrl.getPrepopulatedId() <= 0;
        model.set(SiteSearchDialogProperties.URL_ENABLED, isUrlEditable);
        return model;
    }

    private PropertyModel createAddDialogModel(View customView, SiteSearchDialogMediator mediator) {
        return createDialogModel(
                customView,
                mediator,
                /* titleResId= */ R.string.site_search_dialog_title_add,
                /* positiveButtonResId= */ R.string.site_search_dialog_add);
    }

    private PropertyModel createEditDialogModel(
            View customView, SiteSearchDialogMediator mediator) {
        return createDialogModel(
                customView,
                mediator,
                /* titleResId= */ R.string.site_search_dialog_title_edit,
                /* positiveButtonResId= */ R.string.site_search_dialog_save);
    }

    private PropertyModel createDialogModel(
            View customView,
            SiteSearchDialogMediator mediator,
            int titleResId,
            int positiveButtonResId) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mediator)
                .with(ModalDialogProperties.TITLE, mContext.getString(titleResId))
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(positiveButtonResId))
                .with(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getString(R.string.site_search_dialog_cancel))
                .build();
    }
}
