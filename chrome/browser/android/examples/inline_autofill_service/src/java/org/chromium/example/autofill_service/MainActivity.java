// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.snackbar.Snackbar;

/**
 * Initial page for the installed app. It's unrelated to the AutofillService but provides tools and
 * instructions on how to set it up.
 */
public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);
        setSupportActionBar(findViewById(R.id.toolbar));

        findViewById(R.id.fab)
                .setOnClickListener(
                        v -> {
                            Intent sendIntent = new Intent(Intent.ACTION_APPLICATION_PREFERENCES);
                            sendIntent.addCategory(Intent.CATEGORY_DEFAULT);
                            sendIntent.addCategory(Intent.CATEGORY_APP_BROWSER);
                            sendIntent.addCategory(Intent.CATEGORY_PREFERENCE);

                            // Try to invoke the intent.
                            try {
                                Intent chooser =
                                        Intent.createChooser(sendIntent, "Pick Chrome Channel");
                                startActivity(chooser);
                                Snackbar.make(v, "Triggered the intent", Snackbar.LENGTH_LONG)
                                        .setAnchorView(R.id.fab)
                                        .setAction("Action", null)
                                        .show();
                            } catch (ActivityNotFoundException e) {
                                Snackbar.make(v, "Activity not found", Snackbar.LENGTH_LONG)
                                        .setAnchorView(R.id.fab)
                                        .setAction("Action", null)
                                        .show();
                            }
                        });
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            startActivity(new Intent(this, SettingsActivity.class));
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
