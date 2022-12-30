package com.ark.browser.adblock;

import java.util.ArrayList;
import java.util.List;

public class Rule {

    private boolean exception;
    public RuleOpts opts;
    private List<RulePart> parts = new ArrayList();
    private String raw;

    public Rule(String raw) {
        this.raw = raw;
    }

    public String getRaw() {
        return this.raw;
    }

    public void setRaw(String raw) {
        this.raw = raw;
    }

    public boolean isException() {
        return this.exception;
    }

    public void setException(boolean exception) {
        this.exception = exception;
    }

    public List<RulePart> getParts() {
        return this.parts;
    }

    public void setParts(List<RulePart> parts) {
        this.parts = parts;
    }

    public RuleOpts getOpts() {
        return this.opts;
    }

    public void setOpts(RuleOpts opts) {
        this.opts = opts;
    }

    public boolean hasUnsupportedOpts() {
        if (this.opts == null) {
            return false;
        }
        if (!this.opts.document && this.opts.meida == null && this.opts.popup == null) {
            return false;
        }
        return true;
    }

    public boolean hasContentOpts() {
        if (this.opts == null) {
            return false;
        }
        if (this.opts.image == null && this.opts.object == null && this.opts.script == null && this.opts.stylesheet == null && this.opts.font == null) {
            return false;
        }
        return true;
    }

    public static Rule parseRule(String s) {
        Rule rule = new Rule(s);
        s = s.trim();
        if (s.isEmpty() || s.startsWith("!") || s.startsWith("#")) {
            return null;
        }
        if (s.contains("##")) {
            return null;
        }
        if (s.startsWith("@@")) {
            rule.exception = true;
            s = s.substring(2);
        }
        if (s.startsWith("||")) {
            rule.parts.add(new RulePart(RuleNode.RuleType.DomainAnchor, "||"));
            s = s.substring(2);
        }
        int pos = s.lastIndexOf("$");
        if (pos >= 0) {
            RuleOpts opts = RuleOpts.createRuleOpts(s.substring(pos + 1));
            if (opts == null) {
                return null;
            }
            rule.opts = opts;
            s = s.substring(0, pos);
        }
        while (!s.isEmpty()) {
            pos = indexOfAny(s, "*^|");
            if (pos < 0) {
                rule.parts.add(new RulePart(RuleNode.RuleType.Exact, s));
                return rule;
            }
            if (pos > 0) {
                rule.parts.add(new RulePart(RuleNode.RuleType.Exact, s.substring(0, pos)));
            }
            RuleNode.RuleType type = RuleNode.RuleType.Wildcard;
            switch (s.charAt(pos)) {
                case '*':
                    type = RuleNode.RuleType.Wildcard;
                    break;
                case '^':
                    type = RuleNode.RuleType.Separator;
                    break;
                case '|':
                    type = RuleNode.RuleType.StartAnchor;
                    break;
                default:
                    break;
            }
            rule.parts.add(new RulePart(type, s.substring(pos, pos + 1)));
            s = s.substring(pos + 1);
        }
        return rule;
    }

    private static int indexOfAny(String s, String chars) {
        int pos = -1;
        for (char ch : chars.toCharArray()) {
            int p = s.indexOf(ch);
            if (pos == -1 || (p != -1 && p < pos)) {
                pos = p;
            }
        }
        return pos;
    }

}
