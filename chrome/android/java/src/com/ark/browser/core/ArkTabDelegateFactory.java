// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.core;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_MENU_ID;

import android.app.Activity;
import android.content.res.Resources;
import android.util.Pair;

import androidx.annotation.NonNull;

import com.ark.browser.tab.ArkExternalNavigationDelegateImpl;
import com.ark.browser.tab.ArkTabStateBrowserControlsVisibilityDelegate;
import com.ark.browser.ui.fragment.dialog.ContextMenuDialogFragment;
import com.ark.browser.ui.fragment.dialog.RecyclerAttachDialogFragment;
import com.zpj.fragmentation.dialog.IDialog;
import com.zpj.fragmentation.dialog.base.BaseDialogFragment;
import com.zpj.utils.StatusBarUtils;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator;
import org.chromium.chrome.browser.contextmenu.ContextMenuHeaderCoordinator;
import org.chromium.chrome.browser.contextmenu.ContextMenuHeaderProperties;
import org.chromium.chrome.browser.contextmenu.ContextMenuItemDelegate;
import org.chromium.chrome.browser.contextmenu.ContextMenuNativeDelegate;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link ChromeTabbedActivity}.
 */
public class ArkTabDelegateFactory implements TabDelegateFactory {

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final Supplier<ArkCompositorViewHolder> mCompositorViewHolderSupplier;

    public ArkTabDelegateFactory(BrowserControlsStateProvider browserControlsStateProvider,
                                 FullscreenManager fullscreenManager,
                                 Supplier<ArkCompositorViewHolder> compositorViewHolderSupplier) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mFullscreenManager = fullscreenManager;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ArkTabWebContentsDelegateAndroid(tab,
                mBrowserControlsStateProvider, mFullscreenManager,
                mCompositorViewHolderSupplier);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(new ArkExternalNavigationDelegateImpl(tab));
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return new ArkContextMenuPopulatorFactory(
                new ArkTabContextMenuItemDelegate(tab),
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new ArkTabStateBrowserControlsVisibilityDelegate(tab));
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
    }

    private static class ArkContextMenuPopulatorFactory extends ChromeContextMenuPopulatorFactory {

        BaseDialogFragment<?> mDialog;

        public ArkContextMenuPopulatorFactory(@NonNull ContextMenuItemDelegate itemDelegate, int contextMenuMode) {
            super(itemDelegate, contextMenuMode);
        }

        @Override
        public boolean show(WindowAndroid windowAndroid,
                            WebContents webContents,
                            ContextMenuParams params,
                            List<Pair<Integer, MVCListAdapter.ModelList>> items,
                            ContextMenuNativeDelegate nativeDelegate,
                            Callback<Integer> onItemClicked,
                            Runnable onMenuShown,
                            Runnable onMenuClosed) {
            if (mDialog != null) {
                mDialog.dismiss();
                mDialog = null;
            }

            float density = windowAndroid.getApplicationContext().getResources().getDisplayMetrics().density;
            final float touchPointXPx = params.getTriggeringTouchXDp() * density;
            final float touchPointYPx = params.getTriggeringTouchYDp() * density + StatusBarUtils.getStatusBarHeight();

            Activity activity = windowAndroid.getActivity().get();
            List<MVCListAdapter.ListItem> listItems = getItemList(activity, params, items,
                    onItemClicked, !params.getOpenedFromHighlight());

            mDialog = new ContextMenuDialogFragment()
                    .setOnItemClicked(onItemClicked)
                    .setContextMenuParams(params)
                    .setContextMenuNativeDelegate(nativeDelegate)
                    .addItems(listItems)
                    .setTouchPoint(touchPointXPx, touchPointYPx)
                    .setOnDismissListener(listItemRecyclerAttachDialogFragment -> {
                        if (onMenuClosed != null) {
                            onMenuClosed.run();
                        }
                    })
                    .setOnCancelListener(listItemRecyclerAttachDialogFragment -> {
                        if (onMenuClosed != null) {
                            onMenuClosed.run();
                        }
                    })
                    .show(windowAndroid.getActivity().get());

//            mDialog = ZDialog.attach(MVCListAdapter.ListItem.class)
//                    .addItems(listItems)
//                    .onBindTitle((v, item, position) -> {
//                        v.setText(item.model.get(TEXT));
//                    })
//                    .setOnSelectListener((fragment, position, item) -> {
//                        ZToast.normal((String) item.model.get(TEXT));
////                        if (item.getMenuId() == R.id.contextmenu_freedom_copy) {
////                            String js = "var arr = document.getElementsByTagName('a');\n" +
////                                    "    for(var i = 0; i < arr.length; i++){\n" +
////                                    "        var tag = arr[i];\n" +
////                                    "        if(tag != null && tag.tagName != null){\n" +
////                                    "            tag = tag.tagName.toLocaleLowerCase();\n" +
////                                    "             if(tag != null &&  tag == 'a'){\n" +
////                                    "                 var ele = arr[i];\n" +
////                                    "                 var aHref = ele.getAttribute('href');\n" +
////                                    "                 if(aHref){\n" +
////                                    "                     ele.removeAttribute('href');\n" +
////                                    "                     ele.setAttribute('copyhref',aHref);\n" +
////                                    "                 }\n" +
////
////                                    "             }\n" +
////                                    "         }\n" +
////                                    "    }";
////                            webContents.evaluateJavaScript(js, new JavaScriptCallback() {
////                                @Override
////                                public void handleJavaScriptResult(String jsonResult) {
////                                    ArkLogger.d(this, "jsonResult=" + jsonResult);
////                                }
////                            });
////
////                            ThreadPool.postDelayed(new Runnable() {
////                                @Override
////                                public void run() {
//////                                    controller.setSimulateLongPress(true);
//////                                    controller.setFreedomCopy(true);
//////                                    // TODO 用native实现自由复制
//////                                    ThreadPool.execute(new TouchEventRunnable(touchPointXPx, touchPointYPx, true) {
//////                                        @Override
//////                                        public void run() {
//////                                            super.run();
//////                                            controller.setSimulateLongPress(false);
//////                                        }
//////                                    });
////                                }
////                            }, fragment.getDismissAnimDuration() + 20);
////                        } else {
////
////                            item.model.get(BUTTON_CLICK_LISTENER).onClick(null);
//////                            mPopulator.onItemSelected(ContextMenuHelper.this, params, item);
////                        }
//
//                        item.model.get(BUTTON_CLICK_LISTENER).onClick(null);
//                        fragment.dismiss();
//                    })
//                    .setTouchPoint(touchPointXPx, touchPointYPx)
//                    .show(windowAndroid.getActivity().get());


            return true;
        }

        List<MVCListAdapter.ListItem> getItemList(Activity activity,
                                                  ContextMenuParams params,
                                                  List<Pair<Integer, MVCListAdapter.ModelList>> items,
                                                  Callback<Integer> onItemClicked,
                                                  boolean hasHeader) {
            List<MVCListAdapter.ListItem> itemList = new ArrayList<>();

            // TODO Start with the header
//            if (hasHeader) {
//                PropertyModel model = ContextMenuHeaderCoordinator.buildModel(activity, params);
//                itemList.add(new MVCListAdapter.ListItem(ContextMenuCoordinator.ListItemType.HEADER,
//                        model));
//            }

            for (Pair<Integer, MVCListAdapter.ModelList> group : items) {
                // Add a divider
//                if (group.second.size() > 0) {
//                    itemList.add(new MVCListAdapter.ListItem(
//                            ContextMenuCoordinator.ListItemType.DIVIDER, new PropertyModel()));
//                }
                for (MVCListAdapter.ListItem listItem : group.second) {
                    itemList.add(listItem);
                }
            }

            for (MVCListAdapter.ListItem item : itemList) {
                if (item.type == ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                    item.model.set(BUTTON_CLICK_LISTENER,
                            (v) -> clickItem(item.model.get(BUTTON_MENU_ID), activity, onItemClicked));
                }
            }

            return itemList;
        }

        private void clickItem(int id, Activity activity, Callback<Integer> onItemClicked) {
            // Do not start any action when the activity is on the way to destruction.
            // See https://crbug.com/990987
            if (activity.isFinishing() || activity.isDestroyed()) return;

            onItemClicked.onResult((int) id);
            mDialog.dismiss();
        }

    }

}
