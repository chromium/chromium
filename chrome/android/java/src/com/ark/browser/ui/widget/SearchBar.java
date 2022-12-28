package com.ark.browser.ui.widget;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.AutoCompleteTextView;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import org.chromium.chrome.R;

public class SearchBar extends FrameLayout {

    private AutoCompleteTextView mEditor;
    private ImageView mIvLeft;
    private ImageView mIvRight;

    protected boolean editable;

    private OnSearchListener listener;


    private final OnFocusChangeListener focusChangeListener = new OnFocusChangeListener() {
        @Override
        public void onFocusChange(View v, boolean hasFocus) {
            String input = mEditor.getText().toString();
            if (hasFocus && !TextUtils.isEmpty(input)) {
                mIvRight.setVisibility(View.VISIBLE);
            } else {
                mIvRight.setVisibility(View.GONE);
            }
        }
    };

    public SearchBar(Context context) {
        this(context, null);
    }

    public SearchBar(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SearchBar(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        LayoutInflater.from(context).inflate(R.layout.layout_collections_search_bar, this, true);
        mEditor = findViewById(R.id.tv_edit);
        mIvLeft = findViewById(R.id.iv_left);
        mIvRight = findViewById(R.id.iv_right);

        initEditor();

        mIvRight.setImageResource(R.drawable.ic_clear_black_24dp);
        mIvRight.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mEditor.setText(null);
            }
        });
    }

    public boolean isEditable() {
        return editable;
    }

    private void initEditor() {
        if (editable) {
            mEditor.addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                }

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {

                }

                @Override
                public void afterTextChanged(Editable s) {
                    if (TextUtils.isEmpty(s)) {
                        mIvRight.setVisibility(View.GONE);
                    } else {
                        mIvRight.setVisibility(View.VISIBLE);
                    }
                }
            });
            mEditor.setOnFocusChangeListener(focusChangeListener);
            mEditor.setOnEditorActionListener(new TextView.OnEditorActionListener() {
                @Override
                public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                    if (listener != null && actionId == EditorInfo.IME_ACTION_SEARCH) {
                        if (TextUtils.isEmpty(v.getText().toString())) {
                            Toast.makeText(getContext(), "不能为空！", Toast.LENGTH_SHORT).show();
                            return false;
                        }
                        listener.onSearch(v.getText().toString());
                    }
                    return false;
                }
            });
//            mEditor.setOnClickListener(null);
            mEditor.setFocusable(true);
            mEditor.setCursorVisible(true);
            mEditor.setFocusableInTouchMode(true);
            mEditor.requestFocus();
        } else {
            mIvRight.setVisibility(GONE);
            mEditor.setCursorVisible(false);
            mEditor.clearFocus();
            mEditor.setFocusable(false);
            mEditor.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    SearchBar.this.performClick();
                }
            });
        }
    }

    public void setText(CharSequence text) {
        if (mEditor != null) {
            mEditor.setText(text);
        }
    }

    public void setText(int resId) {
        if (mEditor != null) {
            mEditor.setText(resId);
        }
    }

    public void setTextSize(float size) {
        if (mEditor != null) {
            mEditor.setTextSize(size);
        }
    }

    public void setTextSize(int unit, float size) {
        if (mEditor != null) {
            mEditor.setTextSize(unit, size);
        }
    }

    public void setHintText(CharSequence text) {
        if (mEditor != null) {
            mEditor.setHint(text);
        }
    }

    public void setHintText(int resId) {
        if (mEditor != null) {
            mEditor.setHint(resId);
        }
    }

    public void setHintTextColor(int color) {
        if (mEditor != null) {
            mEditor.setHintTextColor(color);
        }
    }

    public void selectAll() {
        if (mEditor != null) {
            mEditor.selectAll();
        }
    }

    public void setOnSearchListener(OnSearchListener listener) {
        this.listener = listener;
    }

    public void addTextWatcher(TextWatcher textWatcher) {
        if (mEditor != null) {
            mEditor.addTextChangedListener(textWatcher);
        }
    }

    public AutoCompleteTextView getEditor() {
        return mEditor;
    }

    public void setEditable(boolean editable) {
        if (this.editable == editable) {
            return;
        }
        this.editable = editable;
        initEditor();
    }

    public interface OnSearchListener {
        void onSearch(String keyword);
    }

}

