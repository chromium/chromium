package com.ark.browser.adblock;

import android.content.Context;
import android.os.AsyncTask;
import android.text.TextUtils;
import android.util.Log;

import com.ark.browser.utils.FileUtil;
import com.ark.browser.utils.ThreadPool;

import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.tab.Tab;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.Closeable;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class AdblockPlusHelper {

    private static final String FILTER_FILE_NAME = "adblockplus.ini";
    private static final String JS_TO_HIDING_ELEMENT = "function enableAdblock() {var head = document.head;if(head){var adblock_happy = document.getElementById('adblock_happy');if(!adblock_happy){var NewStyles = document.createElement('link');NewStyles.rel = 'stylesheet';NewStyles.type = 'text/css';NewStyles.id = 'adblock_happy';NewStyles.href = 'replaceMeByCss';head.appendChild(NewStyles);}}}enableAdblock();";
    private static final String MARK_AS_AD_FILE = "mark_as_ad.ini";
    private static final String TAG = "AdblockPlusHelper";
    private static boolean sFilterParseBegin = false;
    private static boolean sFilterParseComplete = false;
    public static Map<String, List<MarkAsAd>> sHidingElements = new HashMap<>();
    private static Map<String, List<String>> sHidingSelectors = new HashMap<>();
    private static AdblockPlusHelper sInstance = null;

    public static class MarkAsAd {

        public String host;
        public String srcUrl;
        public String classAttribute;
        public String idAttribute;
        public String tagName;

        MarkAsAd(String host, String tagName, String classAttribute, String idAttribute, String srcUrl) {
            this.host = host;
            this.tagName = tagName;
            this.classAttribute = classAttribute;
            this.idAttribute = idAttribute;
            this.srcUrl = srcUrl;
        }

        public String getRule() {
            return tagName + "#" + idAttribute + "#" + classAttribute + "#" + srcUrl;
        }

        @Override
        public String toString() {
            return host + "#" + tagName + "#" + classAttribute + "#" + idAttribute + "#" + srcUrl;
        }
    }

    public static AdblockPlusHelper getInstance() {
        if (sInstance == null) {
            sInstance = new AdblockPlusHelper();
        }
        return sInstance;
    }

    private AdblockPlusHelper() {
    }

    public boolean match(String url, String originDomain, int resourceType) {
        String contentType = "";
        switch (resourceType) {
            case 2:
                contentType = "css";
                break;
            case 3:
                contentType = "script";
                break;
            case 4:
                contentType = "image/";
                break;
            case 5:
                contentType = "font";
                break;
            default:
                break;
        }
//        Log.d("", "");
        return match(url, originDomain, contentType);
    }

    public boolean match(String url, String originDomain, String contentType) {
        try {
            return match(url, getHostFrom(url), getHostFrom(originDomain), contentType);
        } catch (Exception e) {
            return false;
        }
    }

    public boolean match(String url, String host, String originDomain, String contentType) {
        if (sFilterParseComplete) {
            RuleMatcher ruleMatcher = RuleMatcher.createRuleMatcher();
            MatchRequest request = new MatchRequest();
            request.setUrl(url);
            request.setDomain(host);
            request.setContentType(contentType);
            request.setOriginDomain(originDomain);
            Log.i(TAG, "matchmatchmatch:" + url);
            return ruleMatcher.match(request).ok;
        }
        Log.i(TAG, "Filter has not parsed complete, do nothing for match url:" + url);
        return false;
    }

    public static String getAdblockJs(String selector) {
        return "javascript:" +
                "var ele = document.querySelector('" + selector + "'); " +
                "console.log('ele=' + ele); ele.remove();";
    }

    public static String getAdblockJs(String tag, String id, String srcUrl) {
        String js = "";
        if (!id.isEmpty()){
            js = "javascript:" +
                    "var ele = document.getElementById('" + id + "'); " +
//                    "ele.style.display = 'none'; " +
                    "console.log('ele=' + ele); ele.remove();";
        } else if (!tag.isEmpty() && !srcUrl.isEmpty()) {
            js = "javascript:" +
//                    "function findParentNode(elem) {\n" +
//                    "    if (elem.parentNode.height <= elem.height + 10 || elem.parentNode.width <= elem.width + 10) {\n" +
//                    "        findParentNode(elem.parentNode);\n" +
//                    "    } else {\n" +
//                    "        return elem;\n" +
//                    "    }\n" +
//                    "}" +
                    "var imgList = document.getElementsByTagName('" + tag + "');" +
                    "for (var i = 0; i < imgList.length; i++) {" +
                    "var imgItem = imgList[i];" +
                    "if (" + "'" + srcUrl +  "'" + ".indexOf(imgItem.src) > -1) {" +
//                    "imgItem.style.display = 'none';" +
                    "console.log('imgItem=' + imgItem);  imgItem.remove();" +
                    "console.log('imgItem.parentNode=' + imgItem.parentNode); imgItem.parentNode.remove();" +
//                                "imgItem.parentElement.style.display = 'none';" +
//                                "var parent = imgItem.parentNode" +
//                    "imgItem.parentNode.style.display = 'none';" +//findParentNode(imgItem).style.display = 'none';
//                                "if (imgItem.parentNode.tagName == 'a' || imgItem.parentNode.localName == 'div') {" +
//                                "   imgItem.parentNode.parentNode.removeChild(imgItem.parentNode);" +
//                                "} else {" +
//                                "   imgItem.parentNode.removeChild(imgItem);" +
//                                "}" +
                    "break;" +
                    "}" +
                    "};";
        }
        return js;
    }

    public static void markAds(Tab tab, String url) {
        String host = getHostFrom(url);
        Log.d(TAG, "host=" + host);
        List<MarkAsAd> markAsAdList = sHidingElements.get(host);
        Log.d(TAG, "markAsAdList=null?" + (markAsAdList == null));
        Log.d(TAG, "markAds");
        if (markAsAdList != null) {
            Log.d(TAG, "markAsAdList != null");
            for (MarkAsAd markAsAd : markAsAdList) {
                String js = getAdblockJs(markAsAd.tagName, markAsAd.idAttribute, markAsAd.srcUrl);
                Log.d(TAG, "js=\n" + js);
                tab.getWebContents().evaluateJavaScript(js, null);
            }
        }
    }

//    public static List<MarkAsAd> getAllDomRules() {
//        List<MarkAsAd> res = new ArrayList<>();
//        for (Map.Entry<String, List<MarkAsAd>> entry : sHidingElements.entrySet()) {
//            res.addAll(entry.getValue());
//        }
//        return res;
//    }

    public static List<String> getAllRuleHost() {
        List<String> res = new ArrayList<>(sHidingElements.keySet());
//        Collections.sort(res, new Comparator<String>() {
//            @Override
//            public int compare(String o1, String o2) {
//                return o1.charAt(0) - o2.charAt(0);
//            }
//        });
        Collections.sort(res);
        return res;
    }

    public static List<MarkAsAd> getDomRulesByUrl(String host) {
        List<MarkAsAd> res = sHidingElements.get(host);
        if (res == null) {
            return new ArrayList<>();
        } else {
            return res;
        }
    }

    public String getAdblockJs(String url, Context context) {
        try {
            String hidingSelectors = "";
            String host = getHostFrom(url);
            Log.d("getAdblockJs", "url=" + url + "  host=" + host);
            if (sHidingSelectors.containsKey(host)) {
                hidingSelectors = TextUtils.join(",", sHidingSelectors.get(host).toArray()).replace("\"", "\\\"");
            }
            if (TextUtils.isEmpty(hidingSelectors)) {
                return "";
            }
            return JS_TO_HIDING_ELEMENT.replace("replaceMeByCss", "data:text/css," + hidingSelectors + "{ display: none !important;}");
        } catch (Exception e) {
            return "";
        }
    }

    private String getElementHidingSelectors(String url, Context context) {
        parseFilter(context);
        if (sFilterParseComplete) {
            try {
                String host = getHostFrom(url);
                if (sHidingSelectors.containsKey(host)) {
                    return TextUtils.join(",", sHidingSelectors.get(host).toArray()).replace("\"", "\\\"");
                }
                return "";
            } catch (IllegalArgumentException e) {
                return "";
            }
        }
        Log.i(TAG, "Filter has not parsed complete, do nothing for getElementHidingSelectors");
        return "";
    }

    public static void parseFilter(final Context context) {
        if (sFilterParseBegin) {
            return;
        }
        sFilterParseBegin = true;
        ThreadPool.execute(() -> {
            AdblockPlusHelper.loadRulesFromFile(context);
            sFilterParseComplete = true;
        });
//        try {
//            new AsyncTask<Void, Void, Void>() {
//                public Void doInBackground(Void... unused) {
//                    Log.i(AdblockPlusHelper.TAG, "Begin parse filter.");
//                    Log.i(AdblockPlusHelper.TAG, "Get " + AdblockPlusHelper.loadRulesFromFile(context) + " filters.");
//                    return null;
//                }
//
//                public void onPostExecute(Void unused) {
//                    Log.i(AdblockPlusHelper.TAG, "Complete parse filter.");
//                    sFilterParseComplete = true;
//                }
//            }.execute();
//        } catch (Exception e) {
//        }
    }

    public static int loadRulesFromFile(Context context) {
        String filterFilePath = context.getFilesDir().getPath() + "/" + FILTER_FILE_NAME;
        FileUtil.copyAssetsFile(context, FILTER_FILE_NAME, filterFilePath);
        File file = new File(filterFilePath);
        if (!file.exists()) {
            return 0;
        }
        BufferedReader bReader;
        RuleMatcher m = RuleMatcher.createRuleMatcher();
        List<String> lines = new ArrayList<>();
        try {
            bReader = new BufferedReader(new FileReader(file));
            while (true) {
                String currentLine = bReader.readLine();
                if (currentLine == null) {
                    break;
                }
                lines.add(currentLine);
            }
            bReader.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        int added = 0;
        Log.d(TAG, "loadRulesFromFile--1");
//        for (Rule rule : ) {
//            if (m.addRule(rule, 0)) {
//                added++;
//            }
//        }
        return parseRules(lines, m);
    }

    public static String getHostFrom(String urlString) {
        try {
            URL url = new URL(urlString);
            String host = url.getHost();
            if (host.startsWith("www.")) {
                host = host.substring(4);
            }
            return host;
        } catch (MalformedURLException e2) {

        }
        return urlString;
    }

    private static int parseRules(List<String> lines, RuleMatcher m) {
//        List<Rule> rules = new ArrayList<>();
        boolean foundFilterSection = false;
        Log.d(TAG, "parseRules--1");
        int added = 0;
        for (String line : lines) {
            if (line.startsWith("!")) {
                continue;
            }
            if ("[Subscription filters]".equals(line)) {
                foundFilterSection = true;
            } else if (foundFilterSection && !parseHidingElementSelectors(line)) {
                Log.d(TAG, "parseRules--line=" + line);
                long tempTime = System.currentTimeMillis();
                Rule rule = Rule.parseRule(line);
                Log.d(TAG, "parseRules--parseRuleTime=" + (System.currentTimeMillis() - tempTime));
                if (rule != null) {
//                    rules.add(rule);
                    m.addRule(rule, 0);

                    Log.d(TAG, "parseRules--addRuleTime=" + (System.currentTimeMillis() - tempTime));
                    added++;
                    Log.d(TAG, "parseRules--added=" + added);
                }
            }
        }
        Log.d(TAG, "parseRules--2");
        return added;
    }

    private static boolean parseHidingElementSelectors(String line) {
        if (line.startsWith("!") || !line.contains("##")) {
            return false;
        }
        String[] domainFilter = line.split("##");
        if (domainFilter.length != 2) {
            return false;
        }
        if (!sHidingSelectors.containsKey(domainFilter[0])) {
            sHidingSelectors.put(domainFilter[0], new ArrayList<>());
        }
        sHidingSelectors.get(domainFilter[0]).add(domainFilter[1]);
        return true;
    }

    public static void appendMarkAsAd(final Context context, String pageUrl, String tagName, String classAttribute, String idAttribute, String srcUrl) {
        String[] arrays = new String[]{getHostFrom(pageUrl), tagName, classAttribute, idAttribute, srcUrl};
        final String line = TextUtils.join(NotificationConstants.NOTIFICATION_TAG_SEPARATOR, arrays);
        List<MarkAsAd> markAsAdList = sHidingElements.get(arrays[0]);
        if (markAsAdList == null) {
            sHidingElements.put(arrays[0], new ArrayList<>());
        } else {
            for (MarkAsAd markAsAd : markAsAdList) {
                if (tagName.equals(markAsAd.tagName)
                        && classAttribute.equals(markAsAd.classAttribute)
                        && idAttribute.equals(markAsAd.idAttribute)
                        && srcUrl.equals(markAsAd.srcUrl)) {
                    return;
                }
            }
        }
//        if (!sHidingElements.containsKey(arrays[0])) {
//            sHidingElements.put(arrays[0], new ArrayList<>());
//        } else {
//            List<MarkAsAd> markAsAdList = sHidingElements.get(arrays[0]);
//            for (MarkAsAd markAsAd : markAsAdList) {
//                if (tagName.equals(markAsAd.tagName)
//                        && classAttribute.equals(markAsAd.classAttribute)
//                        && idAttribute.equals(markAsAd.idAttribute)
//                        && srcUrl.equals(markAsAd.srcUrl)) {
//                    return;
//                }
//            }
//
//        }
        sHidingElements.get(arrays[0]).add(new MarkAsAd(arrays[0], arrays[1], arrays[2], arrays[3], arrays[4]));
        try {
            ThreadPool.execute(() -> {
                BufferedWriter bufferedWriter;
                String adFilePath = context.getFilesDir().getPath() + "/" + AdblockPlusHelper.MARK_AS_AD_FILE;
                File file = new File(adFilePath);
                try {
                    if (!file.exists()) {
                        file.createNewFile();
                    }
                    bufferedWriter = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(adFilePath, true)));
                    bufferedWriter.write(line);
                    bufferedWriter.write("\r\n");
                    bufferedWriter.close();
                } catch (IOException e2) {
                    e2.printStackTrace();
                }
            });
//            new AsyncTask<Void, Void, Void>() {
//                public Void doInBackground(Void... unused) {
//
//                    return null;
//                }
//            }.execute();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void loadAdFromFile(final Context context) {
        try {
            ThreadPool.execute(() -> {
                File file = new File(context.getFilesDir().getPath() + "/" + MARK_AS_AD_FILE);
                if (!file.exists()) {
                    return;
                }
                BufferedReader bReader;
                List<String> lines = new ArrayList<>();
                try {
                    bReader = new BufferedReader(new FileReader(file));
                    while (true) {
                        String currentLine = bReader.readLine();
                        if (currentLine == null) {
                            break;
                        }
                        lines.add(currentLine);
                    }
                    bReader.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                for (String line : lines) {
                    String[] arrays = TextUtils.split(line, NotificationConstants.NOTIFICATION_TAG_SEPARATOR);
                    if (!sHidingElements.containsKey(arrays[0])) {
                        sHidingElements.put(arrays[0], new ArrayList<>());
                    }
                    sHidingElements.get(arrays[0]).add(new MarkAsAd(arrays[0], arrays[1], arrays[2], arrays[3], arrays[4]));
                }
            });
//            new AsyncTask<Void, Void, Void>() {
//                public Void doInBackground(Void... unused) {
//
//                    return null;
//
//                }
//            }.execute();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static List<MarkAsAd> getAdElements(String pageUrl) {
        List<MarkAsAd> markAsAds = new ArrayList<>();
        String host = getHostFrom(pageUrl);
        if (host == null || !sHidingElements.containsKey(host)) {
            return markAsAds;
        }
        return sHidingElements.get(host);
    }

    public static void deleteAdElementsByHost(final Context context, final String host, Runnable runnable) {
        sHidingElements.remove(host);
        new AsyncTask<Void, Void, Void>() {
            protected Void doInBackground(Void... unused) {
                try {
                    AdblockPlusHelper.delete(context.getFilesDir().getPath() + "/" + AdblockPlusHelper.MARK_AS_AD_FILE, host);
                } catch (Exception e) {
                    e.printStackTrace();
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                super.onPostExecute(aVoid);
                if (runnable != null) {
                    runnable.run();
                }
            }
        }.execute();
    }

    public static void deleteAdElementsByHost(final Context context, final MarkAsAd rule, Runnable runnable) {
        sHidingElements.get(rule.host).remove(rule);
        if (sHidingElements.get(rule.host).isEmpty()) {
            sHidingElements.remove(rule.host);
        }
        new AsyncTask<Void, Void, Void>() {
            protected Void doInBackground(Void... unused) {
                try {
                    AdblockPlusHelper.deleteRule(new File(context.getFilesDir(), AdblockPlusHelper.MARK_AS_AD_FILE), rule.toString());
                } catch (Exception e) {
                    e.printStackTrace();
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                super.onPostExecute(aVoid);
                if (runnable != null) {
                    runnable.run();
                }
            }
        }.execute();
    }

    private static void delete(String file, String text) {
        delete(new File(file), text);
    }

    private static void deleteRule(File file, String rule) {
        try {
            File temp = File.createTempFile("temp", "temp");
            PrintWriter pw = new PrintWriter(temp);
            BufferedReader br = new BufferedReader(new FileReader(file));
            while (br.ready()) {
                String line = br.readLine();
                System.out.println(line);
                if (TextUtils.equals(line, rule)) {
                    pw.println(line);
                }
            }
            pw.flush();
            safeClose(br);
            safeClose(pw);
            file.delete();
            temp.renameTo(file);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static void delete(File file, String text) {
        try {
            File temp = File.createTempFile("temp", "temp");
            PrintWriter pw = new PrintWriter(temp);
            BufferedReader br = new BufferedReader(new FileReader(file));
            while (br.ready()) {
                String line = br.readLine();
                System.out.println(line);
                if (!line.startsWith(text)) {
                    pw.println(line);
                }
            }
            pw.flush();
            safeClose(br);
            safeClose(pw);
            file.delete();
            temp.renameTo(file);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static void safeClose(Closeable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

}
