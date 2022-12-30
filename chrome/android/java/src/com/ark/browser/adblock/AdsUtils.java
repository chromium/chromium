package com.ark.browser.adblock;

import android.content.Context;
import android.text.TextUtils;

import com.ark.browser.utils.ThreadPool;
import com.zpj.utils.FileUtils;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class AdsUtils {
    private static List<AdRuleItem> nAdRuleList = new ArrayList<>();

    public interface OnGetAdRuleListener {
        void rules(List<AdRuleItem> list);
    }

    private static void initAdRules(Context context) {
        nAdRuleList.clear();
        try {
            for (String textRule2RuleItem : FileUtils.readAssetFile(context, "ad.txt").split("\n")) {
                nAdRuleList.add(textRule2RuleItem(textRule2RuleItem));
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static void getAdRules(Context context, OnGetAdRuleListener onGetAdRuleListener) {
        ThreadPool.execute(() -> {
            if (nAdRuleList == null) {
                initAdRules(context);
            }
            onGetAdRuleListener.rules(nAdRuleList);
        });
    }

    /* JADX WARNING: inconsistent code. */
    /* Code decompiled incorrectly, please refer to instructions dump. */
    public static boolean isAdRule(String s) {
        if (s.length() < 3) {
            return false;
        }
        String substring = s.substring(0, 1);
        String substring2 = s.substring(1, 2);
        return (substring.equals("-")
                && (substring2.equals("-") || substring2.equals(".")))
                || substring.equals("#")
                || getHost(s) != null;
    }

    public static String getHost(String str) {
        for (String Left : new String[]{"/", "#", "-"}) {
            String Left2 = left(str, Left);
            if (!TextUtils.isEmpty(Left2)) {
                return Left2;
            }
        }
        return null;
    }

    public static String left(String str, String str2) {
        String str3 = "";
        if (str != null) {
            if (str2 != null) {
                int index = str.indexOf(str2);
                if (index < 1) {
                    return str3;
                }
                return str.substring(0, index);
            }
        }
        return str3;
    }

//    public static AdSql addRule(String str, String str2) {
//        AdSql adSql = new AdSql();
//        if (str != null) {
//            adSql.setHost(str);
//            adSql.setRule(CnText.Right(str2, str));
//        } else {
//            adSql.setRule(str2);
//        }
//        return adSql;
//    }
//
//    public static AdSql addRule(String str) {
//        return addRule(getHost(str), str);
//    }

    public static AdRuleItem str2RuleItem(String str) {
        AdRuleItem adRuleItem = new AdRuleItem();
        String host = getHost(str);
        if (!TextUtils.isEmpty(host)) {
            adRuleItem.host = host;
            if (TextUtils.isEmpty(right(str, host))) {
                adRuleItem.type = AdRuleType.host;
                return adRuleItem;
            }
        }
        AdRuleItem item = textRule2RuleItem(str);
        adRuleItem.type = item.type;
        adRuleItem.rule = item.rule;
        return adRuleItem;
    }

    public static String right(String str, String str2) {
        int indexOf = str.indexOf(str2);
        if (indexOf == -1) {
            return "";
        }
        return str.substring(indexOf + str2.length(), str.length());
    }


    public static AdRuleItem textRule2RuleItem(String str) {
        AdRuleItem adRuleItem = new AdRuleItem();
        if (!TextUtils.isEmpty(str)) {
            if (str.length() >= 2) {
                String substring = str.substring(0, 2);
                if (substring.equals("##")) {
                    adRuleItem.type = AdRuleType.elemele;
                    adRuleItem.rule = str.substring(2);
                } else if (substring.equals("--")) {
                    adRuleItem.type = AdRuleType.url_con;
                    adRuleItem.rule = str.substring(2);
                } else if (substring.equals("-.")) {
                    adRuleItem.type = AdRuleType.url_hz;
                    adRuleItem.rule = str.substring(2);
                } else if (str.substring(0, 1).equals("/")) {
                    adRuleItem.type = AdRuleType.url_dir;
                    adRuleItem.rule = str;
                } else if (str.substring(0, 1).equals("#")) {
                    adRuleItem.type = AdRuleType.domainName;
                    adRuleItem.host = str.substring(1);
                } else {
                    adRuleItem.type = AdRuleType.host;
                    adRuleItem.host = str;
                }
                return adRuleItem;
            }
        }
        adRuleItem.type = AdRuleType.host;
        adRuleItem.host = str;
        return adRuleItem;
    }

    public static String AdElementHideRule2JsCode(String str) {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("$ls = document.querySelectorAll('");
        stringBuilder.append(str);
        stringBuilder.append("');for (var i = 0;i<$ls.length;i++){ $ls[i].style.visibility = 'hidden'; }");
        return stringBuilder.toString();
    }

}
