// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.ListUpdateCallback;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

/**
 * Custom RecyclerView adapter for instances of {@code AssistantChip}.
 */
public class AssistantChipAdapter extends RecyclerView.Adapter<AssistantChipViewHolder> {
    private final List<AssistantChip> mChips = new ArrayList<>();

    public void setChips(List<AssistantChip> chips) {
        DiffUtil.DiffResult diffResult = DiffUtil.calculateDiff(new DiffUtil.Callback() {
            @Override
            public int getOldListSize() {
                return mChips.size();
            }

            @Override
            public int getNewListSize() {
                return chips.size();
            }

            @Override
            public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
                AssistantChip oldChip = mChips.get(oldItemPosition);
                AssistantChip newChip = chips.get(newItemPosition);
                return newChip.getType() == oldChip.getType()
                        && newChip.getText().equals(oldChip.getText())
                        && newChip.getIcon() == oldChip.getIcon()
                        && newChip.isSticky() == oldChip.isSticky();
            }

            @Override
            public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
                return chips.get(newItemPosition).equals(mChips.get(oldItemPosition));
            }

            @Nullable
            @Override
            public Object getChangePayload(int oldItemPosition, int newItemPosition) {
                return chips.get(newItemPosition);
            }
        });

        // TODO(b/144075373): The following should work, but does not fire change notifications
        //  properly, leading to missing change animations:
        //   diffResult.dispatchUpdatesTo(this);
        // The workaround is to update manually:
        mChips.clear();
        mChips.addAll(chips);
        diffResult.dispatchUpdatesTo(new ListUpdateCallback() {
            @Override
            public void onInserted(int position, int count) {
                for (int i = 0; i < count; ++i) {
                    notifyItemInserted(position);
                }
            }

            @Override
            public void onRemoved(int position, int count) {
                for (int i = 0; i < count; ++i) {
                    notifyItemRemoved(position);
                }
            }

            @Override
            public void onMoved(int fromPosition, int toPosition) {
                notifyItemMoved(fromPosition, toPosition);
            }

            @Override
            public void onChanged(int position, int count, @Nullable Object payload) {
                notifyItemChanged(position);
            }
        });
    }

    @NonNull
    @Override
    public AssistantChipViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return AssistantChipViewHolder.create(parent, viewType);
    }

    @Override
    public void onBindViewHolder(@NonNull AssistantChipViewHolder viewHolder, int position) {
        viewHolder.bind(mChips.get(position));
    }

    @Override
    public void onBindViewHolder(@NonNull AssistantChipViewHolder viewHolder, int position,
            @Nullable List<Object> payloads) {
        // Perform in-place update instead of full bind when possible.
        if (payloads != null && payloads.size() > 0) {
            AssistantChip changedChip = (AssistantChip) payloads.get(payloads.size() - 1);
            viewHolder.getView().setEnabled(!changedChip.isDisabled());
            viewHolder.getView().setVisibility(changedChip.isVisible() ? View.VISIBLE : View.GONE);
            return;
        }
        onBindViewHolder(viewHolder, position);
    }

    @Override
    public int getItemCount() {
        return mChips.size();
    }

    @Override
    public int getItemViewType(int position) {
        return mChips.get(position).getType();
    }
}
