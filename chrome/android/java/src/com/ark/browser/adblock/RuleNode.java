package com.ark.browser.adblock;

import android.text.TextUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class RuleNode {

    private List<RuleNode> children = new ArrayList<>();
    private List<RuleOpts> opts = new ArrayList<>();
    private Pattern pattern = Pattern.compile("^(?:[^\\w\\d_\\-\\.%]|$)");
    private int ruleId;
    private RuleType ruleType;
    private String value;

    class MatchChildrenResult {
        public int ruleId;
        public List<RuleOpts> ruleOptses = new ArrayList<>();

        public MatchChildrenResult(List<RuleOpts> ruleOptses, int ruleId) {
            this.ruleOptses = ruleOptses;
            this.ruleId = ruleId;
        }
    }

    class MatchResult {
        public String error;
        public List<RuleOpts> opts = new ArrayList();
        public int ruleId;

        public MatchResult(int ruleId, List<RuleOpts> opts, String error) {
            this.ruleId = ruleId;
            this.opts = opts;
            this.error = error;
        }
    }

    public enum RuleType {
        Exact,
        Wildcard,
        Separator,
        StartAnchor,
        DomainAnchor,
        Root,
        Substring
    }

    public RuleType getRuleType() {
        return this.ruleType;
    }

    public void setRuleType(RuleType ruleType) {
        this.ruleType = ruleType;
    }

    public String getValue() {
        return this.value;
    }

    public void setValue(String value) {
        this.value = value;
    }

    public List<RuleOpts> getOpts() {
        return this.opts;
    }

    public void setOpts(List<RuleOpts> opts) {
        this.opts = opts;
    }

    public List<RuleNode> getChildren() {
        return this.children;
    }

    public void setChildren(List<RuleNode> children) {
        this.children = children;
    }

    public int getRuleId() {
        return this.ruleId;
    }

    public void setRuleId(int ruleId) {
        this.ruleId = ruleId;
    }

    public boolean addRule(List<RulePart> parts, RuleOpts opts, int id) {
        if (parts.isEmpty()) {
            if (opts != null) {
                this.opts.add(opts);
            }
            this.ruleId = id;
            return true;
        }
        RulePart part = parts.get(0);
        if (part.getType() != RuleType.Exact && part.getType() != RuleType.Wildcard && part.getType() != RuleType.Separator && part.getType() != RuleType.DomainAnchor && part.getType() != RuleType.Substring && part.getType() != RuleType.StartAnchor) {
            return false;
        }
        RuleNode child = null;
        for (RuleNode ruleNode : this.children) {
            if (ruleNode.getRuleType() == part.getType() && ruleNode.value.equals(part.getValue())) {
                child = ruleNode;
                break;
            }
        }
        boolean created = false;
        if (child == null) {
            child = new RuleNode();
            child.setRuleType(part.getType());
            child.setValue(part.getValue());
            created = true;
        }
        boolean result = child.addRule(parts.subList(1, parts.size()), opts, id);
        if (!result || !created) {
            return result;
        }
        this.children.add(child);
        return result;
    }

    public MatchResult match(String url, MatchRequest rq) {
        MatchContext ctx = new MatchContext();
        ctx.setFreq(rq.getCheckFreq());
        ctx.setDuration(rq.getTimeout() == null ? 0 : rq.getTimeout());
        ctx.setGenericBock(rq.hasGenericBlock());
        if (rq.getTimeout() != null && rq.getTimeout() > 0) {
            ctx.setDeadline(System.currentTimeMillis() + rq.getTimeout());
            if (ctx.getFreq() == 0) {
                ctx.setFreq(1000);
            }
        }
        MatchChildrenResult result = dispatch(ctx, url, rq);
        String error = null;
        if (ctx.getLocation() != null && findNodePath(ctx.getLocation(), this) == null) {
            error = "could not find node in rule tree";
        }
        return new MatchResult(result.ruleId, result.ruleOptses, error);
    }

    private MatchChildrenResult matchChildren(MatchContext ctx, String url, MatchRequest rq) {
        if (!ctx.canContinue(this)) {
            return new MatchChildrenResult(null, -1);
        }
        if (this.children.isEmpty() && url.isEmpty()) {
            int domains = 0;
            for (RuleOpts opt : this.opts) {
                domains += opt.domains.size();
                if (!matchOptsDomains(opt, rq.getDomain())) {
                    return new MatchChildrenResult(null, 0);
                }
                if (!matchOptsContent(opt, rq.getContentType())) {
                    return new MatchChildrenResult(null, 0);
                }
                if (!matchOptsThirdParty(opt, rq.getOriginDomain(), rq.getDomain())) {
                    return new MatchChildrenResult(null, 0);
                }
            }
            if (ctx.isGenericBock() && ctx.getIsDomainRule() == 0 && domains == 0) {
                return new MatchChildrenResult(null, 0);
            }
            return new MatchChildrenResult(this.opts, this.ruleId);
        } else if ((this.ruleType == RuleType.Substring || this.ruleType == RuleType.Separator || this.ruleType == RuleType.Wildcard || this.ruleType == RuleType.Exact) && this.children.isEmpty() && this.opts.isEmpty()) {
            return new MatchChildrenResult(this.opts, this.ruleId);
        } else {
            for (RuleNode child : this.children) {
                MatchChildrenResult result = child.dispatch(ctx, url, rq);
                if (result.ruleId < 0) {
                    return result;
                }
                if (result.ruleOptses != null) {
                    return result;
                }
            }
            return new MatchChildrenResult(null, 0);
        }
    }

    private MatchChildrenResult dispatch(MatchContext ctx, String url, MatchRequest rq) {
        MatchChildrenResult result;
        switch (this.ruleType) {
            case Exact:
                if (url.startsWith(this.value)) {
                    return matchChildren(ctx, url.substring(this.value.length()), rq);
                }
                return new MatchChildrenResult(null, 0);
            case Separator:
                Matcher matcher = this.pattern.matcher(url);
                if (matcher.find()) {
                    return matchChildren(ctx, url.substring(matcher.group().length()), rq);
                }
                return new MatchChildrenResult(null, 0);
            case Wildcard:
                if (!this.children.isEmpty()) {
                    if (!url.isEmpty()) {
                        for (int i = 0; i < url.length(); i++) {
                            result = matchChildren(ctx, url.substring(i), rq);
                            if (result.ruleOptses != null || result.ruleId < 0) {
                                return result;
                            }
                        }
                        break;
                    }
                    return matchChildren(ctx, url, rq);
                }
                return matchChildren(ctx, "", rq);
            case DomainAnchor:
                String remaining = matchDomainAnchor(url, this.value);
                if (remaining != null) {
                    ctx.setIsDomainRule(ctx.getIsDomainRule() + 1);
                    result = matchChildren(ctx, remaining, rq);
                    ctx.setIsDomainRule(ctx.getIsDomainRule() - 1);
                    return result;
                }
                break;
            case Root:
                return matchChildren(ctx, url, rq);
            case Substring:
                if (!TextUtils.isEmpty(url)) {
                    int pos = url.indexOf(this.value);
                    if (pos >= 0) {
                        result = matchChildren(ctx, url.substring(this.value.length() + pos), rq);
                        if (result.ruleOptses != null || result.ruleId < 0) {
                            return result;
                        }
                        if (this.opts.isEmpty() && this.children.isEmpty()) {
                            return new MatchChildrenResult(this.opts, this.ruleId);
                        }
                    }
                }
                break;
            case StartAnchor:
                return matchChildren(ctx, url, rq);
        }
        return new MatchChildrenResult(null, 0);
    }

    private String findNodePath(RuleNode target, RuleNode n) {
        if (target == n) {
            return n.getValue();
        }
        for (RuleNode c : n.children) {
            String s = findNodePath(target, c);
            if (s != null) {
                return n.getValue() + s;
            }
        }
        return null;
    }

    private boolean matchOptsDomains(RuleOpts opts, String domain) {
        if (TextUtils.isEmpty(domain)) {
            return true;
        }
        boolean accept = false;
        boolean restrictDomain = false;
        for (String d : opts.domains) {
            String d2 = d;
            boolean reject = d2.startsWith("~");
            if (reject) {
                d2 = d2.substring(1);
            } else {
                restrictDomain = true;
            }
            if (d2.equals(domain) || domain.endsWith("." + d2)) {
                if (reject) {
                    return false;
                }
                accept = true;
            }
        }
        if ((!restrictDomain || !accept) && restrictDomain) {
            return false;
        }
        return true;
    }

    private boolean matchOptsContent(RuleOpts opts, String contentType) {
        if (opts.image != null && contentType.startsWith("image/") != opts.image) {
            return false;
        }
        if (opts.object != null && contentType.startsWith("shockwave") != opts.object) {
            return false;
        }
        if (opts.script != null && contentType.startsWith("script") != opts.script) {
            return false;
        }
        if (opts.stylesheet != null && contentType.startsWith("css") != opts.stylesheet) {
            return false;
        }
        if (opts.font == null || contentType.startsWith("font") == opts.font) {
            return true;
        }
        return false;
    }

    private boolean matchOptsThirdParty(RuleOpts opts, String origin, String domain) {
        if (opts.thirdParty == null) {
            return true;
        }
        boolean isSubDomain;
        if (origin.equals(domain) || domain.endsWith("." + origin)) {
            isSubDomain = true;
        } else {
            isSubDomain = false;
        }
        if (isSubDomain == opts.thirdParty) {
            return false;
        }
        return true;
    }

    private String matchDomainAnchor(String url, String expectedDomain) {
        String s = url;
        if (!s.startsWith("http")) {
            return null;
        }
        s = s.substring(4);
        if (!s.isEmpty() && s.startsWith("s")) {
            s = s.substring(1);
        }
        if (!s.startsWith("://")) {
            return null;
        }
        s = s.substring(3);
        String domain = s;
        int slashPos = s.indexOf("/");
        if (slashPos == -1) {
            s = null;
        } else {
            domain = s.substring(0, slashPos);
            s = s.substring(slashPos);
        }
        for (int i = domain.length() - 1; i > 0; i--) {
            char ch = domain.charAt(i);
            if (ch != '0' && ch != '1' && ch != '2' && ch != '3' && ch != '4' && ch != '5' && ch != '6' && ch != '7' && ch != '8' && ch != '9' && ch != ':') {
                break;
            }
            domain = domain.substring(0, domain.length() - 1);
        }
        if (domain.equals(expectedDomain) || (domain.endsWith(expectedDomain) && domain.length() > expectedDomain.length() && domain.charAt((domain.length() - expectedDomain.length()) - 1) == '.')) {
            return s;
        }
        return null;
    }

}
