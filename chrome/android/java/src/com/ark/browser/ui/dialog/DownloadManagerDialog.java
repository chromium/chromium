package com.ark.browser.ui.dialog;

import android.text.TextUtils;
import android.view.Display;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.ListUpdateCallback;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.ui.widget.AnimProgressBar;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.StringUtils;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.download.DownloadState;
import org.chromium.components.offline_items_collection.ContentId;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class DownloadManagerDialog {

    private static class DownloadListAdapter extends RecyclerView.Adapter<DownloadListViewHolder> {

        private final List<DownloadItem> mDownloadItems = new ArrayList<>();

        protected DownloadListAdapter() {
        }

        public List<DownloadItem> getCurrentList() {
            return mDownloadItems;
        }

        public void submitList(@NonNull List<DownloadItem> newItems) {
            DiffUtil.DiffResult diffResult = DiffUtil.calculateDiff(new DiffUtil.Callback() {
                @Override
                public int getOldListSize() {
                    return mDownloadItems.size();
                }

                @Override
                public int getNewListSize() {
                    return newItems.size();
                }

                @Override
                public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
                    DownloadItem oldItem = mDownloadItems.get(oldItemPosition);
                    DownloadItem newItem = newItems.get(newItemPosition);
                    return TextUtils.equals(oldItem.getId(), newItem.getId());
                }

                @Override
                public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
                    DownloadItem oldItem = mDownloadItems.get(oldItemPosition);
                    DownloadItem newItem = newItems.get(newItemPosition);
                    return TextUtils.equals(oldItem.getId(), newItem.getId())
                            && oldItem.getDownloadInfo().equals(newItem.getDownloadInfo());
                }

                @Nullable
                @Override
                public Object getChangePayload(int oldItemPosition, int newItemPosition) {

                    if (oldItemPosition == newItemPosition) {
                        DownloadItem oldItem = mDownloadItems.get(oldItemPosition);
                        DownloadItem newItem = newItems.get(newItemPosition);
                        if (!areItemsTheSame(oldItemPosition, newItemPosition)) {
                            return null;
                        }
                        DownloadInfo oldInfo = oldItem.getDownloadInfo();
                        DownloadInfo newInfo = newItem.getDownloadInfo();
                        if (oldInfo.state() != newInfo.state()
                                || oldInfo.getFailState() != newInfo.getFailState()
                                || oldInfo.getPendingState() != newInfo.getPendingState()
                                || oldInfo.isPaused() != newInfo.isPaused()) {
                            return "update_state";
                        } else if (oldInfo.getBytesReceived() != newInfo.getBytesReceived()) {
                            return "update_progress";
                        }
                    }

                    return null;
                }
            });

            // TODO(b/144075373): The following should work, but does not fire change notifications
            //  properly, leading to missing change animations:
            //   diffResult.dispatchUpdatesTo(this);
            // The workaround is to update manually:
            mDownloadItems.clear();
            mDownloadItems.addAll(newItems);
            diffResult.dispatchUpdatesTo(new ListUpdateCallback() {
                @Override
                public void onInserted(int position, int count) {
                    ArkLogger.e(DownloadListAdapter.class,
                            "onInserted pos=" + position + " count=" + count);
                    notifyItemRangeInserted(position, count);
                }

                @Override
                public void onRemoved(int position, int count) {
                    ArkLogger.e(DownloadListAdapter.class,
                            "onRemoved pos=" + position + " count=" + count);
                    notifyItemRangeRemoved(position, count);
                }

                @Override
                public void onMoved(int fromPosition, int toPosition) {
                    ArkLogger.e(DownloadListAdapter.class,
                            "onMoved from=" + fromPosition + " to=" + toPosition);
                    notifyItemMoved(fromPosition, toPosition);
                }

                @Override
                public void onChanged(int position, int count, @Nullable Object payload) {
                    ArkLogger.e(DownloadListAdapter.class,
                            "onChanged position=" + position + " count=" + count);
                    notifyItemChanged(position);
                }
            });
        }


        @NonNull
        @Override
        public DownloadListViewHolder onCreateViewHolder(@NonNull ViewGroup viewGroup, int i) {
            ArkLogger.e(DownloadListAdapter.class, "onCreateViewHolder i=" + i);
            View itemView = LayoutInflater.from(viewGroup.getContext()).inflate(R.layout.item_download, viewGroup, false);
            return new DownloadListViewHolder(itemView);
        }

        @Override
        public void onBindViewHolder(@NonNull DownloadListViewHolder holder, int position, @NonNull List<Object> payloads) {
            for (Object payload : payloads) {
                if ("update_state".equals(payload)) {
                    updateState(holder, mDownloadItems.get(position));
                    return;
                } else if ("update_progress".equals(payload)) {
                    updateProgress(holder, mDownloadItems.get(position));
                    return;
                }
            }
            this.onBindViewHolder(holder, position);
        }

        @Override
        public void onBindViewHolder(@NonNull DownloadListViewHolder holder, int i) {
            DownloadItem item = mDownloadItems.get(i);
            DownloadInfo info = item.getDownloadInfo();
            ArkLogger.e(DownloadListAdapter.class, "onBindViewHolder i=" + i
                    + " state=" + info.state()
                    + " failState=" + info.getFailState()
                    + " pendingState=" + info.getPendingState()
                    + "\nname=" + info.getFileName()
                    + "\npath=" + info.getFilePath()
                    + "\nua=" + info.getUserAgent()
                    + "\nurl=" + info.getUrl().getSpec()
                    + "\nitem=" + item
                    + "\ncanResume=" + item.canResume()
                    + "\ninfo.isPaused=" + info.isPaused()
                    + "\nisPaused=" + item.isPaused()
                    + "\ncanPause=" + item.canPause()
                    + "\ncanRetry=" + item.canRetry()
                    + "\n"
            );

            holder.tvName.setText(item.getDownloadInfo().getFileName());


            updateState(holder, item);

            holder.itemView.setOnClickListener(v -> {
                if (item.getDownloadInfo().state() == DownloadState.COMPLETE) {
                    DownloadUtils.openFile(v.getContext(), item);
                } else {
                    Toast.makeText(v.getContext().getApplicationContext(), "TODO click", Toast.LENGTH_SHORT).show();
                }
            });

            holder.itemView.setOnLongClickListener(new View.OnLongClickListener() {
                @Override
                public boolean onLongClick(View v) {
                    return true;
                }
            });

        }

        @Override
        public int getItemCount() {
            return mDownloadItems.size();
        }

        private void updateProgress(DownloadListViewHolder holder, DownloadItem item) {
            holder.tvSize.setText(getProgressText(item));
            if (item.isIndeterminate()) {
                holder.progressBar.setVisibility(View.GONE);
            } else {
                holder.progressBar.setVisibility(View.VISIBLE);
//                holder.progressBar.setMax(100);
                holder.progressBar.setProgress(getPercentage(item), true);
            }
        }

        private void updateState(DownloadListViewHolder holder, DownloadItem item) {
            updateProgress(holder, item);
            DownloadInfo info = item.getDownloadInfo();
            if (item.canResume()) {
                holder.actionButton.setVisibility(View.VISIBLE);
                holder.actionButton.setImageResource(R.drawable.ic_download_pending);
                holder.actionButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        DownloadManagerService.getDownloadManagerService()
                                .resumeDownload(item.getContentId(), item, true);
                    }
                });

                if (item.isFailed()) {
                    holder.tvStatus.setText(StringUtils.getFailStatusForUi(info.getFailState()));
                } else if (item.isPaused()) {
                    holder.tvStatus.setText("Paused");
                }
            } else if (item.canPause()) {
                holder.actionButton.setVisibility(View.VISIBLE);
                holder.actionButton.setImageResource(R.drawable.ic_download_pause);
                holder.actionButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        DownloadManagerService.getDownloadManagerService()
                                .pauseDownload(item.getContentId(), null);
                    }
                });
                if (item.isPending()) {
                    holder.tvStatus.setText("Pending");
                } else {
                    holder.tvStatus.setText(getPercentage(item) + "%");
                }
            } else if (item.canRetry()) {
                holder.actionButton.setVisibility(View.VISIBLE);
                holder.actionButton.setImageResource(R.drawable.ic_refresh);
                holder.actionButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        DownloadManagerService.getDownloadManagerService()
                                .retryDownload(item.getContentId(), item, true);
                    }
                });

                if (item.isFailed()) {
                    holder.tvStatus.setText(StringUtils.getFailStatusForUi(info.getFailState()));
                } else if (item.isCanceled()) {
                    holder.tvStatus.setText("Canceled");
                } else {
                    holder.tvStatus.setText("Unknown");
                }
            } else {
                holder.progressBar.setVisibility(View.GONE);
                holder.actionButton.setVisibility(View.GONE);
                holder.actionButton.setOnClickListener(null);
                holder.tvStatus.setText(getStatusText(item));
            }
        }

        private String getStatusText(DownloadItem item) {
            if (item.isComplete()) {
                return "Complete";
            } else if (item.isPaused()) {
                return "Paused";
            } else if (item.isPending()) {
                return "Pending";
            } else if (item.isCanceled()) {
                return "Canceled";
            } else if (item.isFailed()) {
                return "Failed";
            } else if (item.isDownloading()) {
                return getPercentage(item) + "%";
            } else {
                return "Unknown";
            }
        }

        public String getProgressText(DownloadItem item) {
            String downloaded = org.chromium.components.browser_ui.util.DownloadUtils
                    .getStringForBytes(ContextUtils.getApplicationContext(), item.getDownloadInfo().getBytesReceived());
            String total = org.chromium.components.browser_ui.util.DownloadUtils
                    .getStringForBytes(ContextUtils.getApplicationContext(), item.getDownloadInfo().getBytesTotalSize());
            return total + "/" + downloaded;
        }

        private int getPercentage(DownloadItem item) {
            if (item.isIndeterminate()) {
                return 0;
            }
            return (int) (item.getDownloadInfo().getBytesReceived() * 100 / item.getDownloadInfo().getBytesTotalSize());
        }

    }

    private static class DownloadListViewHolder extends RecyclerView.ViewHolder {

        private TextView tvName;
        private AnimProgressBar progressBar;
        private TextView tvSize;
        private TextView tvStatus;
        private ImageView actionButton;

        public DownloadListViewHolder(View itemView) {
            super(itemView);
            tvName = itemView.findViewById(R.id.tv_name);
            progressBar = itemView.findViewById(R.id.progress_bar);
            tvSize = itemView.findViewById(R.id.tv_size);
            tvStatus = itemView.findViewById(R.id.tv_status);
            actionButton = itemView.findViewById(R.id.btn_action);
        }
    }

    public static void show(ArkBrowserActivity activity) {



        RecyclerView recyclerView = new RecyclerView(activity);
        recyclerView.setLayoutManager(new LinearLayoutManager(activity));

        DownloadListAdapter adapter = new DownloadListAdapter();
        recyclerView.setAdapter(adapter);


        DownloadManagerService.DownloadObserver downloadObserver = new DownloadManagerService.DownloadObserver() {
            @Override
            public void onAllDownloadsRetrieved(List<DownloadItem> list, ProfileKey profileKey) {
                adapter.submitList(new ArrayList<>(list));
                ArkLogger.e(DownloadManagerDialog.class, "onAllDownloadsRetrieved size=" + list.size());
            }

            @Override
            public void onDownloadItemCreated(DownloadItem item) {
                List<DownloadItem> items = new ArrayList<>(adapter.getCurrentList());
                items.add(0, item);
                adapter.submitList(items);
                ArkLogger.e(DownloadManagerDialog.class, "onDownloadItemCreated item=" + item);
            }

            @Override
            public void onDownloadItemUpdated(DownloadItem item) {
                List<DownloadItem> items = new ArrayList<>(adapter.getCurrentList());
                int i = 0;
                for (DownloadItem downloadItem : items) {
                    if (TextUtils.equals(downloadItem.getId(), item.getId())) {
                        ArkLogger.e(DownloadManagerDialog.class, "onDownloadItemUpdated "
                                + "\noldInfo=" + downloadItem.getDownloadInfo()
                                + "\nnewInfo=" + item.getDownloadInfo());
                        items.set(i, item);
                        break;
                    }
                    i++;
                }
                adapter.submitList(items);
                ArkLogger.e(DownloadManagerDialog.class, "onDownloadItemUpdated item=" + item);
            }

            @Override
            public void onDownloadItemRemoved(String guid) {
                List<DownloadItem> items = new ArrayList<>(adapter.getCurrentList());

                Iterator<DownloadItem> it = items.iterator();
                while (it.hasNext()) {
                    DownloadItem item = it.next();
                    if (TextUtils.equals(item.getId(), guid)) {
                        it.remove();
                        break;
                    }
                }
                adapter.submitList(items);
                ArkLogger.e(DownloadManagerDialog.class, "onDownloadItemRemoved guid=" + guid);
            }

            @Override
            public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {
//                List<DownloadItem> items = new ArrayList<>(adapter.getCurrentList());
//                int i = 0;
//                for (DownloadItem downloadItem : items) {
//                    if (downloadItem.getContentId().equals(id)) {
//                        ArkLogger.e(DownloadManagerDialog.class, "onAddOrReplaceDownloadSharedPreferenceEntry "
//                                + "\noldInfo=" + downloadItem.getDownloadInfo());
//                        adapter.notifyItemChanged(i);
//                        break;
//                    }
//                    i++;
//                }
                ArkLogger.e(DownloadManagerDialog.class, "onAddOrReplaceDownloadSharedPreferenceEntry id=" + id);
            }
        };
        DownloadManagerService.getDownloadManagerService().addDownloadObserver(downloadObserver);


        AlertDialog selector = new AlertDialog.Builder(activity)
                .setTitle("Download Manager")
                .setView(recyclerView)
                .setOnCancelListener(dialog -> DownloadManagerService.getDownloadManagerService()
                        .removeDownloadObserver(downloadObserver))
                .setOnDismissListener(dialog -> DownloadManagerService.getDownloadManagerService()
                        .removeDownloadObserver(downloadObserver))
                .create();
        selector.show();

        //设置弹窗在底部
        Window window = selector.getWindow();
        window.setGravity(Gravity.CENTER);

        WindowManager m = activity.getWindowManager();
        Display d = m.getDefaultDisplay(); //为获取屏幕宽、高
        WindowManager.LayoutParams p = selector.getWindow().getAttributes(); //获取对话框当前的参数值
        p.width = d.getWidth(); //宽度设置为屏幕
        p.height = d.getHeight();
        selector.getWindow().setAttributes(p); //设置生效

        DownloadManagerService.getDownloadManagerService().getAllDownloads(null);
    }

}
