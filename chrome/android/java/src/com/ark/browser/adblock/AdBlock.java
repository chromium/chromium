package com.ark.browser.adblock;

import android.content.Context;
import android.content.res.AssetManager;
import android.text.TextUtils;
import android.util.Log;

import com.ark.browser.utils.ThreadPool;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class AdBlock {
    public static final String MIME_TYPE_TEXT_PLAIN = "text/plain";
    public static final String URL_ENCODING = "UTF-8";
    private static final String FILE = "hosts.txt";

    private static final Set<String> hosts = new HashSet<>();
    private static final List<String> whitelist = new ArrayList<>();

    public static void loadHosts(final Context context) {
        ThreadPool.execute(() -> {
            AssetManager manager = context.getAssets();
            try {
                BufferedReader reader = new BufferedReader(new InputStreamReader(manager.open(FILE)));
                String line;
                while ((line = reader.readLine()) != null) {
                    line = line.replace("127.0.0.1", "").trim();
                    if (TextUtils.isEmpty(line)) {
                        continue;
                    }
                    hosts.add(line.toLowerCase());
                }
            } catch (IOException i) {
                Log.w("Browser", "Error loading hosts", i);
            }
        });
    }

    public static Set<String> getHosts() {
        return hosts;
    }

//    private synchronized static void loadDomains(Context context) {
//        RecordAction action = new RecordAction(context);
//        action.open(false);
//        whitelist.clear();
//        whitelist.addAll(action.listDomains());
//        action.close();
//    }

    private static String getDomain(String url) throws URISyntaxException {
        url = url.toLowerCase();

//        int index = url.indexOf('/', 8); // -> http://(7) and https://(8)
//        if (index != -1) {
//            url = url.substring(0, index);
//        }

        URI uri = new URI(url);
        String domain = uri.getHost();
        if (domain == null) {
            return url;
        }
//        return domain.startsWith("www.") ? domain.substring(4) : domain;
        return domain;
    }

    private final Context context;

    public AdBlock(Context context) {
        this.context = context;

        if (hosts.isEmpty()) {
            loadHosts(context);
        }
//        loadDomains(context);
    }

    public boolean isWhite(String url) {
        for (String domain : whitelist) {
            if (url.contains(domain)) {
                return true;
            }
        }
        return false;
    }

    public boolean isAd(String url) {
        if (url.contains("fodm.net/js/ads")) {
            return true;
        }
        String domain;
        try {
            domain = getDomain(url);
        } catch (URISyntaxException u) {
            return false;
        }
        if (hosts.contains(domain)) {
            return true;
        } else if (domain.startsWith("www.")) {
            return hosts.contains(domain.substring(4));
        }
        return false;
    }

//    public synchronized void addDomain(String domain) {
//        RecordAction action = new RecordAction(context);
//        action.open(true);
//        action.addDomain(domain);
//        action.close();
//        whitelist.add(domain);
//    }
//
//    public synchronized void removeDomain(String domain) {
//        RecordAction action = new RecordAction(context);
//        action.open(true);
//        action.deleteDomain(domain);
//        action.close();
//        whitelist.remove(domain);
//    }
//
//    public synchronized void clearDomains() {
//        RecordAction action = new RecordAction(context);
//        action.open(true);
//        action.clearDomains();
//        action.close();
//        whitelist.clear();
//    }
}
