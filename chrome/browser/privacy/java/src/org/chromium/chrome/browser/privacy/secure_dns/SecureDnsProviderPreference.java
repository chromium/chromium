// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.secure_dns;

import android.content.Context;
import android.text.Editable;
import android.text.Html;
import android.text.InputType;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsBridge.Entry;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * SecureDnsProviderPreference is the user interface that is shown when Secure DNS is enabled.
 * When Secure DNS is disabled, the SecureDnsProviderPreference is hidden.
 */
class SecureDnsProviderPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener,
                AdapterView.OnItemSelectedListener,
                TextWatcher {
    // UI strings, loaded from the context.
    private final String mPrivacyTemplate;
    private final String mInvalidWarning;
    private final String mProbeWarning;

    // Server menu entries.
    private final List<Entry> mOptions;

    // UI elements.  These fields are assigned only once, in onBindViewHolder.
    private RadioButtonWithDescriptionLayout mGroup;
    private RadioButtonWithDescription mAutomaticButton;
    private RadioButtonWithDescription mSecureButton;
    private Spinner mServerMenu;
    private TextView mPrivacyPolicy;
    private EditText mCustomServer;
    private TextInputLayout mCustomServerLayout;

    // All variable UI state for SecureDnsProviderPreference is encapsulated in this field.
    // To ensure that the UI is updated whenever the state changes, this field
    // should only be modified by setState().
    private State mState;

    // Checks whether the current template is actually reachable, and updates
    // mCustomServerLayout's error state.
    private final Runnable mProbeRunner = this::startServerProbe;

    /**
     * State is an immutable representation of the control's current UI state.  It can represent
     * states that are invalid, which are required when editing the template or changing modes.
     */
    static class State {
        // Indicates that secure mode is selected.
        public final boolean secure;
        // The selected or entered DoH template(s), if any.
        public final @NonNull String config;
        // Whether the selected template is valid.
        public final boolean valid;

        State(boolean secure, @NonNull String config, boolean valid) {
            this.secure = secure;
            this.config = config;
            this.valid = valid;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof State) {
                State other = (State) obj;
                return other.secure == secure
                        && other.config.equals(config)
                        && other.valid == valid;
            }
            return false;
        }

        @Override
        public int hashCode() {
            // This method is not used, but is defined here for consistency with equals().
            return toString().hashCode();
        }

        State withSecure(boolean secure) {
            return new State(secure, config, valid);
        }

        State withConfig(@NonNull String config) {
            return new State(secure, config, valid);
        }

        State withValid(boolean valid) {
            return new State(secure, config, valid);
        }

        @Override
        public @NonNull String toString() {
            return String.format("State(%b, %s, %b)", secure, config, valid);
        }
    }

    public SecureDnsProviderPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.secure_dns_provider_preference);

        // Preload strings from disk.
        mPrivacyTemplate = context.getString(R.string.settings_secure_dropdown_mode_privacy_policy);
        mInvalidWarning = context.getString(R.string.settings_secure_dns_custom_format_error);
        mProbeWarning = context.getString(R.string.settings_secure_dns_custom_connection_error);
        mOptions = makeOptions(context);
    }

    private static List<Entry> makeOptions(Context context) {
        List<Entry> entries = SecureDnsBridge.getProviders();

        // The Spinner's options consist of an entry called "Custom", followed
        // by the providers in random order.
        List<Entry> options = new ArrayList<>(entries.size() + 1);
        String customEntryName = context.getString(R.string.settings_custom);
        options.add(new Entry(customEntryName, "", ""));
        Collections.shuffle(entries);
        options.addAll(entries);

        return options;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mGroup = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.mode_group);
        mGroup.setOnCheckedChangeListener(this);
        mAutomaticButton = (RadioButtonWithDescription) holder.findViewById(R.id.automatic);
        mSecureButton = (RadioButtonWithDescription) holder.findViewById(R.id.secure);

        View selectionContainer = holder.findViewById(R.id.selection_container);
        mServerMenu = selectionContainer.findViewById(R.id.dropdown_spinner);
        mServerMenu.setOnItemSelectedListener(this);
        Context context = selectionContainer.getContext();
        ArrayAdapter<Entry> adapter =
                new ArrayAdapter<>(context, R.layout.secure_dns_provider_spinner_item, mOptions);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mServerMenu.setAdapter(adapter);

        mPrivacyPolicy = selectionContainer.findViewById(R.id.privacy_policy);
        mPrivacyPolicy.setMovementMethod(LinkMovementMethod.getInstance());
        mCustomServer = selectionContainer.findViewById(R.id.custom_server);
        mCustomServer.addTextChangedListener(this);
        // Show an action button instead of a carriage-return key.
        mCustomServer.setRawInputType(InputType.TYPE_TEXT_VARIATION_URI);
        mCustomServerLayout = selectionContainer.findViewById(R.id.custom_server_layout);

        mGroup.attachAccessoryView(selectionContainer, mSecureButton);
        updateView();
    }

    void setState(State state) {
        if (!state.equals(mState)) {
            mState = state;
            updateView();
        }
    }

    State getState() {
        return mState;
    }

    // Returns the index of the dropdown entry that matches the current template,
    // or 0 if none match (i.e. a custom template).
    private int matchingDropdownIndex() {
        for (int i = 1; i < mServerMenu.getCount(); ++i) {
            Entry entry = (Entry) mServerMenu.getItemAtPosition(i);
            if (entry.config.equals(mState.config)) {
                return i;
            }
        }
        return 0;
    }

    /** Updates the view to match mState. */
    private void updateView() {
        if (mGroup == null) {
            // Not yet bound to view holder.
            return;
        }

        if (mSecureButton.isChecked() != mState.secure) {
            mSecureButton.setChecked(mState.secure);
        }
        boolean automaticMode = !mState.secure;
        if (mAutomaticButton.isChecked() != automaticMode) {
            mAutomaticButton.setChecked(automaticMode);
        }

        int position = matchingDropdownIndex();
        if (mServerMenu.getSelectedItemPosition() != position) {
            mServerMenu.setSelection(position);
        }

        if (mState.secure) {
            mServerMenu.setVisibility(View.VISIBLE);
            // Position 0 is the custom server.  Other positions are actual server entries.
            if (position > 0) {
                // Selected server mode.
                Entry entry = (Entry) mServerMenu.getSelectedItem();
                String html = mPrivacyTemplate.replace("$1", entry.privacy);
                mPrivacyPolicy.setText(Html.fromHtml(html));

                mPrivacyPolicy.setVisibility(View.VISIBLE);
                mCustomServerLayout.setVisibility(View.GONE);
            } else {
                // Custom server mode.
                if (!mCustomServer.getText().toString().equals(mState.config)) {
                    mCustomServer.setText(mState.config);
                    mCustomServer.removeCallbacks(mProbeRunner);
                    if (mState.secure) {
                        mCustomServer.requestFocus();

                        // If the custom server field is idle for one second, run a probe.
                        // Any changes to the field will cancel this probe and start another.
                        mCustomServer.postDelayed(mProbeRunner, 1000);
                    }
                }

                // Show a warning if the input is invalid and is not the start of a valid URL.
                boolean showWarning = !mState.valid && !"https://".startsWith(mState.config);
                mCustomServerLayout.setError(showWarning ? mInvalidWarning : null);

                mCustomServerLayout.setVisibility(View.VISIBLE);
                mPrivacyPolicy.setVisibility(View.GONE);
            }
        } else {
            mServerMenu.setVisibility(View.GONE);
            mPrivacyPolicy.setVisibility(View.GONE);
            mCustomServerLayout.setVisibility(View.GONE);
        }

        SecureDnsBridge.updateValidationHistogram(mState.valid);
    }

    private void startServerProbe() {
        String group = mState.config;
        if (group.isEmpty() || !mState.valid || !mState.secure) {
            return;
        }
        // probeConfig() is a blocking network call that uses WaitableEvent, so it cannot run
        // on the UI thread, nor via the Java PostTask bindings, which do not expose
        // base::WithBaseSyncPrimitives.  Instead, it runs on a fresh Java thread.
        new Thread(
                        () -> {
                            if (SecureDnsBridge.probeConfig(group)) {
                                return;
                            }
                            mCustomServer.post(
                                    () -> { // Send the state change back to the UI thread.
                                        // Check that the setting hasn't been changed.
                                        if (mState.config.contentEquals(group)) {
                                            mCustomServerLayout.setError(mProbeWarning);
                                        }
                                    });
                        })
                .start();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        boolean secure = checkedId == R.id.secure;
        if (mState.secure != secure) {
            tryUpdate(mState.withSecure(secure));
        }
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
        int oldPos = matchingDropdownIndex();
        if (oldPos == pos) {
            // This is the same item that was already in effect.  Ignore spurious event.
            // This check is required to avoid overwriting the custom template, because
            // attaching an adapter triggers a spurious onItemSelected event.
            return;
        }
        Entry entry = (Entry) parent.getItemAtPosition(pos);
        tryUpdate(mState.withConfig(entry.config));
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // In this UI, one radio button is always selected.
    }

    private void tryUpdate(State newState) {
        if (callChangeListener(newState)) {
            setState(newState);
        } else {
            updateView();
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void afterTextChanged(Editable s) {
        tryUpdate(mState.withConfig(s.toString()));

        mCustomServer.removeCallbacks(mProbeRunner);
        mCustomServer.postDelayed(mProbeRunner, 1000);
    }
}
