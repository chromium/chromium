package com.ark.browser.adblock;

import java.util.ArrayList;
import java.util.List;

public class RuleTree {

    private RuleNode root;

    class RewriteResult {
        public String error;
        public List<RulePart> ruleParts = new ArrayList();

        public RewriteResult(List<RulePart> ruleParts, String error) {
            this.ruleParts = ruleParts;
            this.error = error;
        }
    }

    public RuleNode getRoot() {
        return this.root;
    }

    public void setRoot(RuleNode root) {
        this.root = root;
    }

    public static RuleTree createRuleTree() {
        RuleTree ruleTree = new RuleTree();
        ruleTree.setRoot(new RuleNode());
        ruleTree.getRoot().setRuleType(RuleNode.RuleType.Root);
        return ruleTree;
    }

    public boolean addRule(Rule rule, int ruleId) {
        if (rule.hasUnsupportedOpts()) {
            return false;
        }
        RewriteResult rewriteResult = rewriteDomainAnchors(rule.getParts());
        if (rewriteResult.ruleParts != null) {
            List<RulePart> rewritten = replaceWildcardWithSubstring(addLeadingTrailingWildcards(rewriteResult.ruleParts));
            if (rewritten.isEmpty()) {
                return true;
            }
            return this.root.addRule(rewritten, rule.opts, ruleId);
        } else if (rewriteResult.error != null) {
            return false;
        } else {
            return true;
        }
    }

    public RuleNode.MatchResult match(MatchRequest rq) {
        return this.root.match(rq.getUrl(), rq);
    }

    private List<RulePart> addLeadingTrailingWildcards(List<RulePart> parts) {
        List<RulePart> rewritten = new ArrayList();
        for (int i = 0; i < parts.size(); i++) {
            boolean first;
            boolean last;
            if (i == 0) {
                first = true;
            } else {
                first = false;
            }
            if (i == parts.size() - 1) {
                last = true;
            } else {
                last = false;
            }
            RulePart part = (RulePart) parts.get(i);
            if (!(!first || part.getType() == RuleNode.RuleType.StartAnchor || part.getType() == RuleNode.RuleType.DomainAnchor)) {
                rewritten.add(new RulePart(RuleNode.RuleType.Wildcard, ""));
            }
            if (part.getType() != RuleNode.RuleType.StartAnchor) {
                rewritten.add(part);
            } else if (first || last) {
                rewritten.add(part);
            } else {
                rewritten.add(new RulePart(RuleNode.RuleType.Exact, "|"));
            }
            if (last && part.getType() != RuleNode.RuleType.StartAnchor) {
                rewritten.add(new RulePart(RuleNode.RuleType.Wildcard, ""));
            }
        }
        return rewritten;
    }

    private List<RulePart> replaceWildcardWithSubstring(List<RulePart> parts) {
        List<RulePart> rewritten = new ArrayList();
        int i = 0;
        while (i < parts.size()) {
            RulePart part = (RulePart) parts.get(i);
            if (i == 0 || ((RulePart) parts.get(i - 1)).getType() != RuleNode.RuleType.Wildcard) {
                rewritten.add(part);
            } else if (part.getType() != RuleNode.RuleType.Exact) {
                rewritten.add(part);
            } else {
                rewritten.set(rewritten.size() - 1, new RulePart(RuleNode.RuleType.Substring, part.getValue()));
            }
            i++;
        }
        return rewritten;
    }

    private RewriteResult rewriteDomainAnchors(List<RulePart> parts) {
        boolean hasAnchor = false;
        List<RulePart> rewritten = new ArrayList();
        for (int i = 0; i < parts.size(); i++) {
            RulePart part = (RulePart) parts.get(i);
            if (part.getType() == RuleNode.RuleType.DomainAnchor) {
                if (i != 0) {
                    return new RewriteResult(null, "invalid non-starting domain anchor");
                }
                if (parts.size() < 2 || ((RulePart) parts.get(1)).getType() != RuleNode.RuleType.Exact) {
                    return new RewriteResult(null, "domain anchor must be followed by exact match");
                }
                hasAnchor = true;
            } else if (part.getType() == RuleNode.RuleType.Exact && hasAnchor) {
                String value = part.getValue();
                String domain = "";
                int slashPos = value.indexOf("/");
                if (slashPos != -1) {
                    domain = value.substring(0, slashPos);
                    value = value.substring(slashPos);
                } else {
                    domain = value;
                    value = "";
                }
                rewritten.set(rewritten.size() - 1, new RulePart(RuleNode.RuleType.DomainAnchor, domain));
                if (!value.isEmpty()) {
                    rewritten.add(new RulePart(RuleNode.RuleType.Exact, value));
                }
                hasAnchor = false;
            }
            rewritten.add(part);
        }
        return new RewriteResult(rewritten, null);
    }

}
