package com.ark.browser.adblock;

public class RuleMatcher {

    private static RuleMatcher sInstance = null;
    private RuleTree contentExcludes;
    private RuleTree contentIncludes;
    private RuleTree excludes;
    private RuleTree genericBlock;
    private RuleTree includes;

    class MatchResult {
        public String error;
        public boolean ok;
        public int ruleId;

        public MatchResult(boolean ok, int ruleId, String error) {
            this.ok = ok;
            this.ruleId = ruleId;
            this.error = error;
        }
    }

    public RuleTree getIncludes() {
        return this.includes;
    }

    public void setIncludes(RuleTree includes) {
        this.includes = includes;
    }

    public RuleTree getExcludes() {
        return this.excludes;
    }

    public void setExcludes(RuleTree excludes) {
        this.excludes = excludes;
    }

    public RuleTree getContentIncludes() {
        return this.contentIncludes;
    }

    public void setContentIncludes(RuleTree contentIncludes) {
        this.contentIncludes = contentIncludes;
    }

    public RuleTree getContentExcludes() {
        return this.contentExcludes;
    }

    public void setContentExcludes(RuleTree contentExcludes) {
        this.contentExcludes = contentExcludes;
    }

    public RuleTree getGenericBlock() {
        return this.genericBlock;
    }

    public void setGenericBlock(RuleTree genericBlock) {
        this.genericBlock = genericBlock;
    }

    public static RuleMatcher createRuleMatcher() {
        if (sInstance == null) {
            sInstance = new RuleMatcher();
            sInstance.setIncludes(RuleTree.createRuleTree());
            sInstance.setExcludes(RuleTree.createRuleTree());
            sInstance.setContentExcludes(RuleTree.createRuleTree());
            sInstance.setContentIncludes(RuleTree.createRuleTree());
            sInstance.setGenericBlock(RuleTree.createRuleTree());
        }
        return sInstance;
    }

    public static RuleMatcher createRuleMatcherForTest() {
        RuleMatcher ruleMatcher = new RuleMatcher();
        ruleMatcher.setIncludes(RuleTree.createRuleTree());
        ruleMatcher.setExcludes(RuleTree.createRuleTree());
        ruleMatcher.setContentExcludes(RuleTree.createRuleTree());
        ruleMatcher.setContentIncludes(RuleTree.createRuleTree());
        ruleMatcher.setGenericBlock(RuleTree.createRuleTree());
        return ruleMatcher;
    }

    public boolean addRule(Rule rule, int ruleId) {
        if (rule.opts == null || !rule.opts.genericBlock) {
            RuleTree ruleTree;
            if (rule.hasContentOpts()) {
                if (rule.isException()) {
                    ruleTree = this.contentExcludes;
                } else {
                    ruleTree = this.contentIncludes;
                }
            } else if (rule.isException()) {
                ruleTree = this.excludes;
            } else {
                ruleTree = this.includes;
            }
            return ruleTree.addRule(rule, ruleId);
        } else if (rule.isException()) {
            return this.genericBlock.addRule(rule, ruleId);
        } else {
            return false;
        }
    }

    public MatchResult match(MatchRequest rq) {
        RuleNode.MatchResult matchResult;
        boolean z = true;
        boolean copied = false;
        if (rq.isGenericBlock() == null) {
            matchResult = this.genericBlock.match(rq);
            if (matchResult.error != null) {
                return new MatchResult(false, 0, matchResult.error);
            }
            if (matchResult.opts != null) {
                copied = true;
                rq.setGenericBlock(Boolean.TRUE);
            }
        }
        RuleTree inc = this.includes;
        RuleTree exc = this.excludes;
        if (!rq.getContentType().isEmpty()) {
            inc = this.contentIncludes;
            exc = this.contentExcludes;
        }
        matchResult = inc.match(rq);
        if (matchResult.opts == null || matchResult.error != null) {
            return new MatchResult(false, 0, matchResult.error);
        }
        if (copied) {
            rq.setGenericBlock(null);
        }
        matchResult = exc.match(rq);
        if (matchResult.opts != null) {
            z = false;
        }
        return new MatchResult(z, matchResult.ruleId, matchResult.error);
    }

}
