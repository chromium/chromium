package com.ark.browser.core;

import android.text.TextUtils;

import androidx.annotation.NonNull;

import com.ark.browser.core.utils.ContentUtils;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.PrefsHelper;

import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class UserAgentManager {

    public static class UserAgent {

        private final Map<String, String> brandVersionMap = new HashMap<>();
        private final Map<String, String> fullBrandVersionMap = new HashMap<>();

        private final String name;
        private final String userAgent;
        private final boolean isMobile;

        private String fullVersion;
        private String platform;
        private String platformVersion;
        private String architecture;
        private String model;
        private String bitness;
        private boolean wow64 = false;

        public UserAgent(String name, String userAgent, boolean isMobile) {
            this.name = name;
            this.userAgent = userAgent;
            this.isMobile = isMobile;
        }

        public String getName() {
            return name;
        }

        public String getString() {
            return userAgent;
        }

        public boolean isMobile() {
            return isMobile;
        }

        @Override
        public String toString() {
            return "UserAgent{" +
                    "userAgent='" + userAgent + '\'' +
                    ", isMobile=" + isMobile +
                    '}';
        }
    };

    private static final UserAgent[] ARRAY_USER_AGENT = {
            new UserAgent("安卓UA", ContentUtils.getBrowserUserAgent(), true),
            new UserAgent("桌面UA", "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.230 Safari/537.36 Vivaldi/2.2.1388.37", false),
            new UserAgent("苹果UA", "Mozilla/5.0 (iPhone 6s; CPU iPhone OS 11_4_1 like Mac OS X) AppleWebKit/604.3.5 (KHTML, like Gecko) Version/11.0 MQQBrowser/8.3.0 Mobile/15B87 Safari/604.1 MttCustomUA/2 QBWebViewType/1 WKType/1", true),
            new UserAgent("塞班UA", "Mozilla/5.0 (SymbianOS/9.4;Series60/5.0 Nokia5230-1b/21.2 .005; Profile/MIDP-2.1 Config uration/CLDC-1.1 ) AppleWebKit/525 (KHTML, like Gecko) Version/3.0 BrowserNG/7.2.5.2 3gpp-gba", true),
            new UserAgent("黑莓UA", "Mozilla/5.0 (BlackBerry;U;BlackBerry9800;en) AppleWebKit/534.1+ (KHTML,likeGecko) Version/6.0.0.337 Mobile Safari/534.1+", true)
    };

    private static final ConcurrentHashMap<String, Integer> HOST_UA_MAPPING = new ConcurrentHashMap<>();

    private static int sDefaultUserAgentIndex = -1;

    public static UserAgent[] getUserAgentArray() {
        return ARRAY_USER_AGENT;
    }

    public static String[] getUserAgentNames() {
        String[] items = new String[ARRAY_USER_AGENT.length];
        for (int i = 0; i < items.length; i++) {
            items[i] = ARRAY_USER_AGENT[i].name;
        }
        return items;
    }

    public static UserAgent getDefaultUserAgent() {
        return getUserAgent(getDefaultUserAgentIndex());
    }

    public static int getDefaultUserAgentIndex() {
        if (sDefaultUserAgentIndex < 0) {
            sDefaultUserAgentIndex = getUserAgentIndexByUrl("default");
        }
        return sDefaultUserAgentIndex;
    }

    public static void setDefaultUserAgentIndex(int index) {
        index = Math.max(0, index);
        sDefaultUserAgentIndex = index;
        setUserAgentByUrl("default", index);

//        PrefsHelper.with().applyInt("current_user_agent_position", pos);
    }

    public static UserAgent getUserAgent(int pos) {
        if (pos < 0) {
            return ARRAY_USER_AGENT[getDefaultUserAgentIndex()];
        }
        return ARRAY_USER_AGENT[pos];
    }

    public static void setUserAgentByUrl(String host, int index) {
        long start = System.currentTimeMillis();
        HOST_UA_MAPPING.put(host, index);
        PrefsHelper.with("user_agent_manager").applyInt(host, index);
        ArkLogger.d(UserAgentManager.class, "setUserAgentByUrl deltaTime=" + (System.currentTimeMillis() - start)
                + " host=" + host + " index=" + index);
    }

    public static int getUserAgentIndexByUrl(@NonNull GURL url) {
        return getUserAgentIndexByUrl(url.getHost());
    }

    public static int getUserAgentIndexByUrl(String host) {
        long start = System.currentTimeMillis();
        Integer index = HOST_UA_MAPPING.get(host);
        if (index == null) {
            if ("default".equals(host)) {
                index = PrefsHelper.with("user_agent_manager").getInt(host, 0);
                sDefaultUserAgentIndex = index;
            } else {
                index = PrefsHelper.with("user_agent_manager").getInt(host, -1);
                if (index < 0) {
                    index = getDefaultUserAgentIndex();
                }
            }
            HOST_UA_MAPPING.put(host, index);
        }
        ArkLogger.d(UserAgentManager.class, "getUserAgentIndexByUrl deltaTime="
                + (System.currentTimeMillis() - start) + " host=" + host + " index=" + index);
        return index;
    }

    public static UserAgent getUserAgentByUrl(@NonNull GURL url) {
        long start = System.currentTimeMillis();
        String host = url.getHost();
        if (TextUtils.isEmpty(host)) {
            return getDefaultUserAgent();
        }
        UserAgent ua = getUserAgent(getUserAgentIndexByUrl(host));
        ArkLogger.d(UserAgentManager.class, "getUserAgentByUrl deltaTime="
                + (System.currentTimeMillis() - start) + " host=" + host + " ua=" + ua);
        return ua;
    }


}
