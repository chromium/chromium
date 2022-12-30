package com.ark.browser.adblock;

import android.text.TextUtils;

import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.ArrayList;
import java.util.List;

public class RuleOpts {

    public Boolean collapse;
    public boolean document;
    public List<String> domains = new ArrayList();
    public boolean elemHide;
    public Boolean font;
    public boolean genericBlock;
    public boolean genericHide;
    public Boolean image;
    public Boolean meida;
    public Boolean object;
    public Boolean objectSubRequest;
    public Boolean other;
    public Boolean ping;
    public Boolean popup;
    public String raw;
    public Boolean script;
    public Boolean stylesheet;
    public Boolean subDocument;
    public Boolean thirdParty;
    public Boolean xmlHttpRequest;

    public RuleOpts(String raw) {
        this.raw = raw;
    }

    public static RuleOpts createRuleOpts(String raw) {
        int i = 0;
        RuleOpts opts = new RuleOpts(raw);
        for (String opt : TextUtils.split(raw, ",")) {
            String opt2 = opt.trim();
            boolean value = true;
            if (opt2.startsWith("~")) {
                value = false;
                opt2 = opt2.substring(1);
            }
            if (opt2.startsWith("domain=")) {
                String[] split = TextUtils.split(opt2.substring(7), "\\|");
                int length = split.length;
                while (i < length) {
                    opts.domains.add(split[i].trim());
                    i++;
                }
                return opts;
            }
            int i2 = -1;
            switch (opt2.hashCode()) {
                case -1112093552:
                    if (opt2.equals("xmlhttprequest")) {
                        i2 = 14;
                        break;
                    }
                    break;
                case -1023368385:
                    if (opt2.equals("object")) {
                        i2 = 3;
                        break;
                    }
                    break;
                case -991966464:
                    if (opt2.equals("third-party")) {
                        i2 = 12;
                        break;
                    }
                    break;
                case -907685685:
                    if (opt2.equals("script")) {
                        i2 = 0;
                        break;
                    }
                    break;
                case -632085587:
                    if (opt2.equals("collapse")) {
                        i2 = 17;
                        break;
                    }
                    break;
                case -260159210:
                    if (opt2.equals("genericblock")) {
                        i2 = 10;
                        break;
                    }
                    break;
                case -8255151:
                    if (opt2.equals("elemhide")) {
                        i2 = 9;
                        break;
                    }
                    break;
                case 3148879:
                    if (opt2.equals("font")) {
                        i2 = 18;
                        break;
                    }
                    break;
                case 3441010:
                    if (opt2.equals("ping")) {
                        i2 = 13;
                        break;
                    }
                    break;
                case 100313435:
                    if (opt2.equals("image")) {
                        i2 = 1;
                        break;
                    }
                    break;
                case 103772132:
                    if (opt2.equals("media")) {
                        i2 = 15;
                        break;
                    }
                    break;
                case 106069776:
                    if (opt2.equals("other")) {
                        i2 = 6;
                        break;
                    }
                    break;
                case 106852524:
                    if (opt2.equals("popup")) {
                        i2 = 16;
                        break;
                    }
                    break;
                case 158213710:
                    if (opt2.equals("stylesheet")) {
                        i2 = 2;
                        break;
                    }
                    break;
                case 268257181:
                    if (opt2.equals("object-subrequest")) {
                        i2 = 4;
                        break;
                    }
                    break;
                case 615002447:
                    if (opt2.equals("object_subrequest")) {
                        i2 = 5;
                        break;
                    }
                    break;
                case 861720859:
                    if (opt2.equals(UrlConstants.DOCUMENT_SCHEME)) {
                        i2 = 8;
                        break;
                    }
                    break;
                case 1100161945:
                    if (opt2.equals("generichide")) {
                        i2 = 11;
                        break;
                    }
                    break;
                case 2111590235:
                    if (opt2.equals("subdocument")) {
                        i2 = 7;
                        break;
                    }
                    break;
            }
            switch (i2) {
                case 0:
                    opts.script = Boolean.valueOf(value);
                    break;
                case 1:
                    opts.image = Boolean.valueOf(value);
                    break;
                case 2:
                    opts.stylesheet = Boolean.valueOf(value);
                    break;
                case 3:
                    opts.object = Boolean.valueOf(value);
                    break;
                case 4:
                case 5:
                    opts.objectSubRequest = Boolean.valueOf(value);
                    break;
                case 6:
                    opts.other = Boolean.valueOf(value);
                    break;
                case 7:
                    opts.subDocument = Boolean.valueOf(value);
                    break;
                case 8:
                    opts.document = value;
                    break;
                case 9:
                    opts.elemHide = value;
                    break;
                case 10:
                    opts.genericBlock = value;
                    break;
                case 11:
                    opts.genericHide = value;
                    break;
                case 12:
                    opts.thirdParty = Boolean.valueOf(value);
                    break;
                case 13:
                    opts.ping = Boolean.valueOf(value);
                    break;
                case 14:
                    opts.xmlHttpRequest = Boolean.valueOf(value);
                    break;
                case 15:
                    opts.meida = Boolean.valueOf(value);
                    break;
                case 16:
                    opts.popup = Boolean.valueOf(value);
                    break;
                case 17:
                    opts.collapse = Boolean.valueOf(value);
                    break;
                case 18:
                    opts.font = Boolean.valueOf(value);
                    break;
                default:
                    return null;
            }
        }
        return opts;
    }

}
