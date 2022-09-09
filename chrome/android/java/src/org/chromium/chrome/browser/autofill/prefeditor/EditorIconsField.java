// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.prefeditor;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;

import java.util.List;

/**
 * Helper class for creating a horizontal list of icons with a title.
 */
class EditorIconsField {
    private final View mLayout;

    /**
     * Builds a horizontal list of icons.
     *
     * @param context    The application context to use when creating widgets.
     * @param root       The object that provides a set of LayoutParams values for the view.
     * @param fieldModel The data model of the icon list.
     */
    public EditorIconsField(Context context, ViewGroup root, EditorFieldModel fieldModel) {
        assert fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_ICONS;

        mLayout = LayoutInflater.from(context).inflate(
                R.layout.editable_option_editor_icons, root, false);

        ((TextView) mLayout.findViewById(R.id.label)).setText(fieldModel.getLabel());

        ExpandableGridView iconsContainer =
                (ExpandableGridView) mLayout.findViewById(R.id.icons_container);
        iconsContainer.setAdapter(new IconListAdapter(context, fieldModel.getIconResourceIds(),
                fieldModel.getIconDescriptionsForAccessibility()));
    }

    /** @return The View containing everything. */
    public View getLayout() {
        return mLayout;
    }

    /**
     * An instance of a {@link BaseAdapter} that provides a list of card icon views.
     */
    private static class IconListAdapter extends BaseAdapter {
        private final Context mContext;
        private final List<Integer> mIconResourceIds;
        private final List<Integer> mIconDescriptionIds;
        private final int mIconSize;
        private final int mHorizontalPaddingInPixels;
        private final int mVerticalPaddingInPixels;

        public IconListAdapter(
                Context context, List<Integer> iconResourceIds, List<Integer> iconDescriptionIds) {
            mContext = context;
            mIconResourceIds = iconResourceIds;
            mIconDescriptionIds = iconDescriptionIds;
            mIconSize = mContext.getResources().getDimensionPixelSize(
                    R.dimen.editable_option_section_logo_width);
            mHorizontalPaddingInPixels = mContext.getResources().getDimensionPixelSize(
                    R.dimen.editable_option_section_logo_horizontal_padding);
            mVerticalPaddingInPixels = mContext.getResources().getDimensionPixelSize(
                    R.dimen.editable_option_section_logo_vertical_padding);
            assert mIconResourceIds.size() == mIconDescriptionIds.size();
        }

        @Override
        public int getCount() {
            return mIconResourceIds.size();
        }

        @Override
        public Object getItem(int position) {
            return mIconResourceIds.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ImageView iconView = (ImageView) convertView;
            if (iconView == null) iconView = new ImageView(mContext);
            iconView.setImageDrawable(
                    AppCompatResources.getDrawable(mContext, mIconResourceIds.get(position)));
            iconView.setContentDescription(mContext.getString(mIconDescriptionIds.get(position)));
            iconView.setAdjustViewBounds(true);
            iconView.setMaxWidth(mIconSize);
            iconView.setMaxHeight(mIconSize);
            iconView.setPadding(mHorizontalPaddingInPixels, mVerticalPaddingInPixels,
                    mHorizontalPaddingInPixels, mVerticalPaddingInPixels);
            return iconView;
        }
    }
}
